/*
 * Copyright (c) 2012 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2007 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arch/x86/pagetable_walker.hh"

#include <memory>

#include "arch/x86/faults.hh"
#include "arch/x86/pagetable.hh"
#include "arch/x86/tlb.hh"
#include "base/bitfield.hh"
#include "base/trie.hh"
#include "cpu/base.hh"
#include "cpu/thread_context.hh"
#include "debug/PageTableWalker.hh"
#include "mem/packet_access.hh"
#include "mem/request.hh"

namespace gem5
{
#define NVM_USER_REG_START 0x120001000

namespace X86ISA {

Fault
Walker::start(ThreadContext * _tc, BaseMMU::Translation *_translation,
              const RequestPtr &_req, BaseMMU::Mode _mode)
{
    // TODO: in timing mode, instead of blocking when there are other
    // outstanding requests, see if this request can be coalesced with
    // another one (i.e. either coalesce or start walk)
    WalkerState * newState = new WalkerState(this, _translation, _req);
    newState->initState(_tc, _mode, sys->isTimingMode());
    if (currStates.size()) {
        assert(newState->isTiming());
        DPRINTF(PageTableWalker, "Walks in progress: %d\n", currStates.size());
        currStates.push_back(newState);
        return NoFault;
    } else {
        currStates.push_back(newState);
        Fault fault = newState->startWalk();
        if (!newState->isTiming()) {
            currStates.pop_front();
            delete newState;
        }
        return fault;
    }
}

Fault
Walker::startFunctional(ThreadContext * _tc, Addr &addr, unsigned &logBytes,
              BaseMMU::Mode _mode)
{
    funcState.initState(_tc, _mode);
    return funcState.startFunctional(addr, logBytes);
}

bool
Walker::WalkerPort::recvTimingResp(PacketPtr pkt)
{
   if (pkt->getTracker()){
       walker->ssp_packet_received += 1;
       delete pkt;
       return true;
   }
   return walker->recvTimingResp(pkt);
}

bool
Walker::recvTimingResp(PacketPtr pkt)
{
    WalkerSenderState * senderState =
        dynamic_cast<WalkerSenderState *>(pkt->popSenderState());
    WalkerState * senderWalk = senderState->senderWalk;
    bool walkComplete = senderWalk->recvPacket(pkt);
    delete senderState;
    if (walkComplete) {
        std::list<WalkerState *>::iterator iter;
        for (iter = currStates.begin(); iter != currStates.end(); iter++) {
            WalkerState * walkerState = *(iter);
            if (walkerState == senderWalk) {
                iter = currStates.erase(iter);
                break;
            }
        }
        delete senderWalk;
        // Since we block requests when another is outstanding, we
        // need to check if there is a waiting request to be serviced
        if (currStates.size() && !startWalkWrapperEvent.scheduled())
            // delay sending any new requests until we are finished
            // with the responses
            schedule(startWalkWrapperEvent, clockEdge());
    }
    return true;
}

void
Walker::WalkerPort::recvReqRetry()
{
    walker->recvReqRetry();
}

void
Walker::recvReqRetry()
{
    if (retryingbitmap){
        while (writesbitmap.size()) {
            //std::cout<<"bitmap write retry"<<std::endl;
            PacketPtr write = writesbitmap.back();
            writesbitmap.pop_back();
            if (!port.sendTimingReq(write)){
                retryingbitmap = true;
                writesbitmap.push_back(write);
                break;
            }
            else{
                ssp_packet_send += 1;
            }
            retryingbitmap = false;
        }
    }
    std::list<WalkerState *>::iterator iter;
    for (iter = currStates.begin(); iter != currStates.end(); iter++) {
        WalkerState * walkerState = *(iter);
        if (walkerState->isRetrying()) {
            walkerState->retry();
        }
    }
}

bool Walker::sendTiming(WalkerState* sendingState, PacketPtr pkt)
{
    WalkerSenderState* walker_state = new WalkerSenderState(sendingState);
    pkt->pushSenderState(walker_state);
    if (port.sendTimingReq(pkt)) {
        return true;
    } else {
        // undo the adding of the sender state and delete it, as we
        // will do it again the next time we attempt to send it
        pkt->popSenderState();
        delete walker_state;
        return false;
    }

}

Port &
Walker::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "port")
        return port;
    else
        return ClockedObject::getPort(if_name, idx);
}

void
Walker::WalkerState::initState(ThreadContext * _tc,
        BaseMMU::Mode _mode, bool _isTiming)
{
    assert(state == Ready);
    started = false;
    tc = _tc;
    mode = _mode;
    timing = _isTiming;
    walker->bitmap_address = 0;
    if (timing){
        //std::cout<<"inside"<<std::endl;
        walker->bitmap_address = tc->readMiscRegNoEffect(
                        gem5::X86ISA::MISCREG_DIRTYMAP_ADDR);
       // walker->bitmap_address = 0;
    }
}

void
Walker::startWalkWrapper()
{
    unsigned num_squashed = 0;
    WalkerState *currState = currStates.front();
    while ((num_squashed < numSquashable) && currState &&
        currState->translation->squashed()) {
        currStates.pop_front();
        num_squashed++;

        DPRINTF(PageTableWalker, "Squashing table walk for address %#x\n",
            currState->req->getVaddr());

        // finish the translation which will delete the translation object
        currState->translation->finish(
            std::make_shared<UnimpFault>("Squashed Inst"),
            currState->req, currState->tc, currState->mode);

        // delete the current request if there are no inflight packets.
        // if there is something in flight, delete when the packets are
        // received and inflight is zero.
        if (currState->numInflight() == 0) {
            delete currState;
        } else {
            currState->squash();
        }

        // check the next translation request, if it exists
        if (currStates.size())
            currState = currStates.front();
        else
            currState = NULL;
    }
    if (currState && !currState->wasStarted())
        currState->startWalk();
}

Fault
Walker::WalkerState::startWalk()
{
    Fault fault = NoFault;
    assert(!started);
    started = true;
    setupWalk(req->getVaddr());
    if (timing) {
        nextState = state;
        state = Waiting;
        timingFault = NoFault;
        sendPackets();
    } else {
        do {
            walker->port.sendAtomic(read);
            PacketPtr write = NULL;
            fault = stepWalk(write);
            assert(fault == NoFault || read == NULL);
            state = nextState;
            nextState = Ready;
            if (write)
                walker->port.sendAtomic(write);
        } while (read);
        state = Ready;
        nextState = Waiting;
    }
    return fault;
}

Fault
Walker::WalkerState::startFunctional(Addr &addr, unsigned &logBytes)
{
    Fault fault = NoFault;
    assert(!started);
    started = true;
    setupWalk(addr);

    do {
        walker->port.sendFunctional(read);
        // On a functional access (page table lookup), writes should
        // not happen so this pointer is ignored after stepWalk
        PacketPtr write = NULL;
        fault = stepWalk(write);
        assert(fault == NoFault || read == NULL);
        state = nextState;
        nextState = Ready;
    } while (read);
    logBytes = entry.logBytes;
    addr = entry.paddr;

    return fault;
}

Fault
Walker::WalkerState::stepWalk(PacketPtr &write)
{
    assert(state != Ready && state != Waiting);
    Fault fault = NoFault;
    write = NULL;
    PageTableEntry pte;
    static PageTableEntry pte_my;
    unsigned long ssp_offset = 0;
    if (state == LongSSP1 || state==LongSSP2 || state==LongSSP3){
        //std::cout<<"SSP"<<std::endl;
        pte = pte_my;
    }
    else{
        if (dataSize == 8)
            pte = read->getLE<uint64_t>();
        else
            pte = read->getLE<uint32_t>();
    }
    VAddr vaddr = entry.vaddr;
    //std::cout<<"stepwalk vaddr: "<<std::hex<<vaddr<<std::endl;
    bool uncacheable = pte.pcd;
    Addr nextRead = 0;
    Addr write_address = 0;
    bool doWrite = false;
    bool doTLBInsert = false;
    bool doEndWalk = false;
    bool badNX = pte.nx && mode == BaseMMU::Execute && enableNX;
    switch(state) {
      case LongPML4:
        DPRINTF(PageTableWalker,
                "Got long mode PML4 entry %#016x.\n", (uint64_t)pte);
        nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.longl3 * dataSize;
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        entry.noExec = pte.nx;
        nextState = LongPDP;
        break;
      case LongPDP:
        DPRINTF(PageTableWalker,
                "Got long mode PDP entry %#016x.\n", (uint64_t)pte);
        nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.longl2 * dataSize;
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        nextState = LongPD;
        break;
      case LongPD:
        DPRINTF(PageTableWalker,
                "Got long mode PD entry %#016x.\n", (uint64_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if (!pte.ps) {
            // 4 KB page
            entry.logBytes = 12;
            nextRead =
                ((uint64_t)pte & (mask(40) << 12)) + vaddr.longl1 * dataSize;
            nextState = LongPTE;
            break;
        } else {
            // 2 MB page
            entry.logBytes = 21;
            entry.paddr = (uint64_t)pte & (mask(31) << 21);
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            entry.vaddr = entry.vaddr & ~((2 * (1 << 20)) - 1);
            doTLBInsert = true;
            doEndWalk = true;
            break;
        }
      case LongPTE:
        DPRINTF(PageTableWalker,
                "Got long mode PTE entry %#016x.\n", (uint64_t)pte);
        pte_my = pte;
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        if (!pte.d && entry.writable){
            pte.d = 1;
            doWrite = true;
            DPRINTF(PageTableWalker,
                "Got long mode PTE setting dirty bit.\n");
        }
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        entry.paddr = (uint64_t)pte & (mask(40) << 12);
        entry.uncacheable = uncacheable;
        entry.global = pte.g;
        entry.patBit = bits(pte, 12);
        entry.vaddr = entry.vaddr & ~((4 * (1 << 10)) - 1);
        //doTLBInsert = true;
        //doEndWalk = true;
        if (walker->bitmap_address>0 && (entry.paddr >= NVM_USER_REG_START)){
            nextState = LongSSP1;
            ssp_offset = ((entry.paddr&~(0xfff))-NVM_USER_REG_START)>>12;
            struct ssp_entry* temp_entry =
                    (struct ssp_entry*)(walker->bitmap_address+
                                    (ssp_offset*sizeof(struct ssp_entry)));
            nextRead = (Addr)&temp_entry->p1;
        }else{
            doTLBInsert = true;
            doEndWalk = true;
        }
        break;
      case LongSSP1:
        //std::cout<<"SSP1 "<<std::endl;
        if (walker->bitmap_address>0 && (entry.paddr >= NVM_USER_REG_START)){
            nextState = LongSSP2;
            Addr p1;
            read->getData((uint8_t *)&p1);
            entry.p1 = p1;
            ssp_offset = ((entry.paddr&~(0xfff))-NVM_USER_REG_START)>>12;
            struct ssp_entry* temp_entry =
                    (struct ssp_entry*)(walker->bitmap_address+
                                    (ssp_offset*sizeof(struct ssp_entry)));
            nextRead = (Addr)&temp_entry->current_bitmap;
            //std::cout<<"next address: "<<std::hex<<nextRead<<std::endl;
            //std::cout<<"p0: "<<std::hex<<entry.paddr<<std::endl;
            //std::cout<<"p1: "<<std::hex<<entry.p1<<std::endl;
        }else{
            doTLBInsert = true;
            doEndWalk = true;
        }
        //doTLBInsert = true;
        //doEndWalk = true;
        //doWrite = false;
        break;
      case LongSSP2:
        //std::cout<<"SSP2 "<<std::endl;
        if (walker->bitmap_address>0 && (entry.paddr >= NVM_USER_REG_START)){
            nextState = LongSSP3;
            uint64_t current_bitmap;
            read->getData((uint8_t *)&current_bitmap);
            entry.current_bitmap = current_bitmap;
            ssp_offset = ((entry.paddr&~(0xfff))-NVM_USER_REG_START)>>12;
            struct ssp_entry* temp_entry =
                    (struct ssp_entry*)(walker->bitmap_address+
                                    (ssp_offset*sizeof(struct ssp_entry)));
            nextRead = (Addr)&temp_entry->updated_bitmap;
            //std::cout<<"next address: "<<std::hex<<nextRead<<std::endl;
        }else{
            doTLBInsert = true;
            doEndWalk = true;
        }
        //doTLBInsert = true;
        //doEndWalk = true;
        //doWrite = false;
        break;
      case LongSSP3:
        //std::cout<<"SSP3: "<<std::endl;
        if (walker->bitmap_address>0 && (entry.paddr >= NVM_USER_REG_START)){
            uint64_t updated_bitmap;
            read->getData((uint8_t *)&updated_bitmap);
            entry.updated_bitmap = updated_bitmap;
            ssp_offset = ((entry.paddr&~(0xfff))-NVM_USER_REG_START)>>12;
            struct ssp_entry* temp_entry =
                    (struct ssp_entry*)(walker->bitmap_address+
                                    (ssp_offset*sizeof(struct ssp_entry)));
            write_address = (Addr)&(temp_entry->evicted);
            //std::cout<<"updated bitmap: "<<updated_bitmap<<std::endl;
        }
        doTLBInsert = true;
        doEndWalk = true;
        doWrite = false;
        break;
      case PAEPDP:
        DPRINTF(PageTableWalker,
                "Got legacy mode PAE PDP entry %#08x.\n", (uint32_t)pte);
        nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.pael2 * dataSize;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        nextState = PAEPD;
        break;
      case PAEPD:
        DPRINTF(PageTableWalker,
                "Got legacy mode PAE PD entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if (!pte.ps) {
            // 4 KB page
            entry.logBytes = 12;
            nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.pael1 * dataSize;
            nextState = PAEPTE;
            break;
        } else {
            // 2 MB page
            entry.logBytes = 21;
            entry.paddr = (uint64_t)pte & (mask(31) << 21);
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            entry.vaddr = entry.vaddr & ~((2 * (1 << 20)) - 1);
            doTLBInsert = true;
            doEndWalk = true;
            break;
        }
      case PAEPTE:
        DPRINTF(PageTableWalker,
                "Got legacy mode PAE PTE entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        entry.paddr = (uint64_t)pte & (mask(40) << 12);
        entry.uncacheable = uncacheable;
        entry.global = pte.g;
        entry.patBit = bits(pte, 7);
        entry.vaddr = entry.vaddr & ~((4 * (1 << 10)) - 1);
        doTLBInsert = true;
        doEndWalk = true;
        break;
      case PSEPD:
        DPRINTF(PageTableWalker,
                "Got legacy mode PSE PD entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if (!pte.ps) {
            // 4 KB page
            entry.logBytes = 12;
            nextRead =
                ((uint64_t)pte & (mask(20) << 12)) + vaddr.norml2 * dataSize;
            nextState = PTE;
            break;
        } else {
            // 4 MB page
            entry.logBytes = 21;
            entry.paddr = bits(pte, 20, 13) << 32 | bits(pte, 31, 22) << 22;
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            entry.vaddr = entry.vaddr & ~((4 * (1 << 20)) - 1);
            doTLBInsert = true;
            doEndWalk = true;
            break;
        }
      case PD:
        DPRINTF(PageTableWalker,
                "Got legacy mode PD entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        // 4 KB page
        entry.logBytes = 12;
        nextRead = ((uint64_t)pte & (mask(20) << 12)) + vaddr.norml2 * dataSize;
        nextState = PTE;
        break;
      case PTE:
        DPRINTF(PageTableWalker,
                "Got legacy mode PTE entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        entry.paddr = (uint64_t)pte & (mask(20) << 12);
        entry.uncacheable = uncacheable;
        entry.global = pte.g;
        entry.patBit = bits(pte, 7);
        entry.vaddr = entry.vaddr & ~((4 * (1 << 10)) - 1);
        doTLBInsert = true;
        doEndWalk = true;
        break;
      default:
        panic("Unknown page table walker state %d!\n");
    }
    if (doEndWalk) {
        PacketPtr oldRead = read;
         if (!functional && doWrite) {
            write = oldRead;
            write->setLE<uint64_t>(pte);
            write->cmd = MemCmd::WriteReq;
            read = NULL;
        } else {
            write = NULL;
        }
        if (state == LongSSP3){
        //change entry as not evicted in ssp cache
            unsigned ssp_evict = 0;
            Request::Flags ssp_flags = Request::PHYSICAL;
            assert(write_address > 0);
            RequestPtr ssp_req = std::make_shared<Request>(
                            write_address, sizeof(unsigned), ssp_flags,
                            walker->getrequestorId());
            PacketPtr ssp_write = new Packet(ssp_req, MemCmd::WriteReq);
            ssp_write->allocate();
            ssp_write->setTracker(1);
            ssp_write->setData((uint8_t*)&ssp_evict);
            walker->sendTimingbitmap(ssp_write);
        }
        if (doTLBInsert)
            if (!functional)
                walker->tlb->insert(entry.vaddr, entry);
        endWalk();
    } else {
        PacketPtr oldRead = read;
        //If we didn't return, we're setting up another read.
        Request::Flags flags = oldRead->req->getFlags();
        flags.set(Request::UNCACHEABLE, uncacheable);
        if (state == LongPTE || state==LongSSP1 || state==LongSSP2){
            //std::cout<<"next address: "<<std::hex<<nextRead<<std::endl;
            RequestPtr request = std::make_shared<Request>(
                            nextRead,8, flags, walker->requestorId);
            read = new Packet(request, MemCmd::ReadReq);
            read->allocate();
        }
        else{
           RequestPtr request = std::make_shared<Request>(
             nextRead, oldRead->getSize(), flags, walker->requestorId);
           read = new Packet(request, MemCmd::ReadReq);
           read->allocate();
        }
        // If we need to write, adjust the read packet to write the modified
        // value back to memory.
        if (doWrite) {
            write = oldRead;
            write->setLE<uint64_t>(pte);
            write->cmd = MemCmd::WriteReq;
        } else {
            write = NULL;
            delete oldRead;
        }
    }
    return fault;
}

void
Walker::WalkerState::endWalk()
{
    nextState = Ready;
    delete read;
    read = NULL;
}

void
Walker::WalkerState::setupWalk(Addr vaddr)
{
    VAddr addr = vaddr;
    CR3 cr3 = tc->readMiscRegNoEffect(MISCREG_CR3);
    // Check if we're in long mode or not
    Efer efer = tc->readMiscRegNoEffect(MISCREG_EFER);
    dataSize = 8;
    Addr topAddr;
    if (efer.lma) {
        // Do long mode.
        state = LongPML4;
        topAddr = (cr3.longPdtb << 12) + addr.longl4 * dataSize;
        enableNX = efer.nxe;
    } else {
        // We're in some flavor of legacy mode.
        CR4 cr4 = tc->readMiscRegNoEffect(MISCREG_CR4);
        if (cr4.pae) {
            // Do legacy PAE.
            state = PAEPDP;
            topAddr = (cr3.paePdtb << 5) + addr.pael3 * dataSize;
            enableNX = efer.nxe;
        } else {
            dataSize = 4;
            topAddr = (cr3.pdtb << 12) + addr.norml2 * dataSize;
            if (cr4.pse) {
                // Do legacy PSE.
                state = PSEPD;
            } else {
                // Do legacy non PSE.
                state = PD;
            }
            enableNX = false;
        }
    }

    nextState = Ready;
    entry.vaddr = vaddr;

    Request::Flags flags = Request::PHYSICAL;
    if (cr3.pcd)
        flags.set(Request::UNCACHEABLE);

    RequestPtr request = std::make_shared<Request>(
        topAddr, dataSize, flags, walker->requestorId);

    read = new Packet(request, MemCmd::ReadReq);
    read->allocate();
}

bool
Walker::WalkerState::recvPacket(PacketPtr pkt)
{
    assert(pkt->isResponse());
    assert(inflight);
    assert(state == Waiting);
    inflight--;
    if (squashed) {
        // if were were squashed, return true once inflight is zero and
        // this WalkerState will be freed there.
        return (inflight == 0);
    }
    if (pkt->isRead()) {
        // should not have a pending read it we also had one outstanding
        assert(!read);

        // @todo someone should pay for this
        pkt->headerDelay = pkt->payloadDelay = 0;

        state = nextState;
        nextState = Ready;
        PacketPtr write = NULL;
        read = pkt;
        timingFault = stepWalk(write);
        state = Waiting;
        assert(timingFault == NoFault || read == NULL);
        if (write) {
            writes.push_back(write);
        }
        sendPackets();
    } else {
        sendPackets();
    }
    if (inflight == 0 && read == NULL && writes.size() == 0) {
        state = Ready;
        nextState = Waiting;
        if (timingFault == NoFault) {
            /*
             * Finish the translation. Now that we know the right entry is
             * in the TLB, this should work with no memory accesses.
             * There could be new faults unrelated to the table walk like
             * permissions violations, so we'll need the return value as
             * well.
             */
            bool delayedResponse;
            Fault fault = walker->tlb->translate(req, tc, NULL, mode,
                                                 delayedResponse, true);
            assert(!delayedResponse);
            // Let the CPU continue.
            translation->finish(fault, req, tc, mode);
        } else {
            // There was a fault during the walk. Let the CPU know.
            translation->finish(timingFault, req, tc, mode);
        }
        return true;
    }

    return false;
}

void
Walker::WalkerState::sendPackets()
{
    //If we're already waiting for the port to become available, just return.
    if (retrying)
        return;

    //Reads always have priority
    if (read) {
        PacketPtr pkt = read;
        read = NULL;
        inflight++;
        if (!walker->sendTiming(this, pkt)) {
            retrying = true;
            read = pkt;
            inflight--;
            return;
        }
    }
    //Send off as many of the writes as we can.
    while (writes.size()) {
        PacketPtr write = writes.back();
        writes.pop_back();
        inflight++;
        if (!walker->sendTiming(this, write)) {
            retrying = true;
            writes.push_back(write);
            inflight--;
            return;
        }
    }
}

unsigned
Walker::WalkerState::numInflight() const
{
    return inflight;
}

bool
Walker::WalkerState::isRetrying()
{
    return retrying;
}

bool
Walker::isRetryingbitmap()
{
    return retryingbitmap;
}


bool
Walker::WalkerState::isTiming()
{
    return timing;
}

bool
Walker::WalkerState::wasStarted()
{
    return started;
}

void
Walker::WalkerState::squash()
{
    squashed = true;
}

void
Walker::WalkerState::retry()
{
    retrying = false;
    sendPackets();
}

Fault
Walker::WalkerState::pageFault(bool present)
{
    DPRINTF(PageTableWalker, "Raising page fault.\n");
    HandyM5Reg m5reg = tc->readMiscRegNoEffect(MISCREG_M5_REG);
    if (mode == BaseMMU::Execute && !enableNX)
        mode = BaseMMU::Read;
    return std::make_shared<PageFault>(entry.vaddr, present, mode,
                                       m5reg.cpl == 3, false);
}

} // namespace X86ISA
} // namespace gem5
