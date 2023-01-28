#ifndef _APIC_H_
#define IA32_APIC_BASE_MSR 0x1B  /*MSR base --> contains the APIC base address*/

#define APIC_ID_OFFSET 0x20
#define APIC_VERSION_OFFSET 0x30
#define APIC_TPR_OFFSET 0x80
#define APIC_EOI_OFFSET 0xB0
#define APIC_LDR_OFFSET 0xD0
#define APIC_DFR_OFFSET 0xE0
#define APIC_SPURIOUS_VECTOR_OFFSET 0xF0 
#define APIC_ERROR_STATUS_OFFSET 0x280 
#define APIC_LVT_TIMER_OFFSET 0x320 
#define APIC_LVT_PMU_OFFSET 0x340 
#define APIC_LVT_LINT0_OFFSET 0x350 
#define APIC_LVT_LINT1_OFFSET 0x360 
#define APIC_LVT_ERROR_OFFSET 0x370 
#define APIC_TIMER_INIT_COUNT_OFFSET 0x380 
#define APIC_TIMER_CURRENT_COUNT_OFFSET 0x390 
#define APIC_TIMER_DIVIDE_CONFIG_OFFSET 0x3E0 

#define UINT32_MAX  (0xffffffff)

#define APIC_INTERVAL 300

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

extern u64 elapsed(u32 start_hi, u32 start_lo, u32 end_hi, u32 end_lo);
extern void init_apic(void);
extern void install_apic_mapping(u64 pl4);
extern void remove_apic_mapping(u64 pl4);
extern int is_apic_base(u64);
extern void ack_irq();
extern void reset_timer(u32);
extern u32 calibrate_apic_timer(void);
#endif
