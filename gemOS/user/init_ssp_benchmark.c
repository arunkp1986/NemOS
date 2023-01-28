#include<ulib.h>

#define LLC_SIZE (1<<21)
#define NUM_ACCESS 10

#define MILLI_SEC 3000000 //3000000 cycles
#define STACK_ELEM  5120 // 20480 bytes, 5 pages
#define STACK_SIZE 20480 // 5 pages 
#define HEAP_SIZE 40960 // 40960, 10 pages 
#define HEAP_ELEM 10240 // 40960, 10 pages 
#define REC_COUNT 1000000 // number of operations

unsigned lfsr_rand(unsigned seed){
    unsigned lfsr = seed;
    u16 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}

#define RDTSC_START()            \
        __asm__ volatile("RDTSCP\n\t" \
                         "mov %%edx, %0\n\t" \
                         "mov %%eax, %1\n\t" \
                         : "=r" (start_hi), "=r" (start_lo) \
                         :: "%rax", "%rbx", "%rcx", "%rdx");

#define RDTSC_STOP()              \
        __asm__ volatile("RDTSCP\n\t" \
                         "mov %%edx, %0\n\t" \
                         "mov %%eax, %1\n\t" \
                         "CPUID\n\t" \
                         : "=r" (end_hi), "=r" (end_lo) \
                         :: "%rax", "%rbx", "%rcx", "%rdx");

u64 uelapsed(u32 start_hi, u32 start_lo, u32 end_hi, u32 end_lo)
{
        u64 start = (((u64)start_hi) << 32) | start_lo;
        u64 end   = (((u64)end_hi)   << 32) | end_lo;
        return end-start;
}

struct data{
        unsigned interval;
        unsigned offset;
        char ops;
        int size;
        int type;
    };

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    u32 start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u32 target_count = MILLI_SEC*5; //values at 5 millisec interval
    u32 loop_counter = 1024;
    u64 rdtsc_count = 0;
    register int count = 0;
   /* do{
        count = 0;
	rdtsc_count = 0;
	loop_counter = (loop_counter<<1);
        RDTSC_START();    
        while(count < loop_counter){
            count += 1;
	} 
        RDTSC_STOP();
        rdtsc_count = uelapsed(start_hi,start_lo,end_hi,end_lo);
    }while(rdtsc_count<target_count);
    printf("counter:%u rdtsc:%u\n",loop_counter,rdtsc_count);
*/
    u32 array[STACK_ELEM];
    u32* heap1 = (u32 *)mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_NVM);
    u32* heap2 = (u32 *)mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_NVM);
    unsigned size = sizeof(struct data)*REC_COUNT;
    unsigned num_blocks = ((size>>12)<<12)==size?size>>12:(size>>12)+1;
    char* input = (char *)mmap(NULL, num_blocks*4096, PROT_READ|PROT_WRITE,MAP_POPULATE);
    for(int blk_off = 0; blk_off < num_blocks; blk_off++){
        read_blk((char*)((u64)input+(blk_off<<12)), blk_off);
    }
    struct data *dt = (struct data*)input;
    int i = 0;
    int prev_interval = 0;
    gem5_dump_stats();
    gem5_reset_stats();
    checkpoint_start();
    for(int outer=0; outer<4; outer++){
        unsigned start = outer*(REC_COUNT>>2);
	unsigned end = (outer+1)*(REC_COUNT>>2);
    for(i=start; i<end; i++){
        //printf("Interval: %u, offset:%u, ops:%c, size:%d, type:%d\n", 
	//		dt->interval, dt->offset, dt->ops, dt->size, dt->type);
	/*if(prev_interval != dt->interval){
            count = 0;
	    while(count < loop_counter){
                count += 1;
	    }
	    prev_interval = dt->interval;
	}*/
	//printf("iteration:%d\n",i);
	if(dt->type == 0){
            if(dt->type == 'R'){
	        u32 offset = dt->offset%STACK_ELEM;
		u32 curr_size = dt->size/4;
		offset = offset - curr_size;
		for(int j=0; j<curr_size; j++){
		     u32 value = *(u32*)(array+offset+j);
		}
	    }
	    if(dt->type == 'W'){
	        u32 offset = dt->offset%STACK_ELEM;
		u32 curr_size = dt->size/4;
		offset = offset - curr_size;
		for(int j=0; j<curr_size; j++){
		     *(u32*)(array+offset+j) = 1;
		}
	    }
	}
	else if(dt->type == 1){
            if(dt->type == 'R'){
	        u32 offset = dt->offset%HEAP_ELEM;
		u32 curr_size = dt->size/4;
		offset = offset - curr_size;
		for(int j=0; j<curr_size; j++){
		     u32 value = *(u32*)(heap1+offset+j);
		}
	    }
	    if(dt->type == 'W'){
	        u32 offset = dt->offset%HEAP_ELEM;
		u32 curr_size = dt->size/4;
		offset = offset - curr_size;
		for(int j=0; j<curr_size; j++){
		     *(u32*)(heap1+offset+j) = 1;
		}
	    }
	}
	else if(dt->type == 2){
            if(dt->type == 'R'){
	        u32 offset = dt->offset%HEAP_ELEM;
		u32 curr_size = dt->size/4;
		offset = offset - curr_size;
		for(int j=0; j<curr_size; j++){
		     u32 value = *(u32*)(heap2+offset+j);
		}
	    }
	    if(dt->type == 'W'){
	        u32 offset = dt->offset%HEAP_ELEM;
		u32 curr_size = dt->size/4;
		offset = offset - curr_size;
		for(int j=0; j<curr_size; j++){
		     *(u32*)(heap2+offset+j) = 1;
		}
	    }
	}
	
	dt += 1;
    }
        if(outer != 3){
            gem5_dump_stats();
            gem5_reset_stats();
	}
    }
    checkpoint_end();
    gem5_dump_stats();
    gem5_reset_stats();
    //printf("checkpoints:%u\n",checkpoint_stats());
    return 0;
}
