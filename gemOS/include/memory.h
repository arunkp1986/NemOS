#ifndef __MEMORY_H_
#define __MEMORY_H_

#include<types.h>
#include <list.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_2MB_SHIFT 21

#define HYBRID 1

#define PGD_MASK 0xff8000000000UL 
#define PUD_MASK 0x007fc0000000UL
#define PMD_MASK 0x00003fe00000UL
#define PTE_MASK 0x0000001ff000UL

#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12


#define FLAG_MASK 0x3ffffffff000UL 


// #define OS_PT_MAPS 64    /*XXX USER_REGION bitmap is @ 100MB, so we must map atleast 128MB*/

#define REGION_OS_DS_START 0x800000
#define REGION_OS_PT_START 0x2000000
#define REGION_USER_START  0x6400000
#define REGION_FILE_DS_START    0x20000000
#define REGION_FILE_STORE_START 0x22000000
#define ENDMEM                  0x80000000


#define REGION_DRAM_START 0x00000000
#define REGION_DRAM_END   0x80000000
#define REGION_NVM_START  0x100000000
#define REGION_NVM_ENDMEM 0x180000000

#define NVM_META_REG_START REGION_NVM_START
#define NVM_USER_REG_MAGIC (REGION_NVM_START+0x20000000)
#define NVM_USER_REG_START (NVM_USER_REG_MAGIC+0x1000)

#define OS_PT_MAPS 1024   /*XXX Total PMD entries (vmem = OS_PT_MAPS*2MB) USER_REGION bitmap is @ 100MB, so we must map atleast 128MB*/


#define MAP_RD  0x0
#define MAP_WR  0x1

#define NUM_PAGES 0x80000

extern struct list_head * os_chunk_alloc_list;
extern struct list_head * ssp_bitmap_list;
enum{
         OS_DS_REG,
         OS_PT_REG,
         USER_REG,
         FILE_DS_REG,
         FILE_STORE_REG,
	 NVM_META_REG,
	 NVM_MAGIC_REG,
	 NVM_USER_REG,
         MAX_REG
};

enum{
        OSALLOC_32,
        OSALLOC_64,
        OSALLOC_128,
        OSALLOC_256,
        OSALLOC_512,
        OSALLOC_1024,
        OSALLOC_2048,
        OSALLOC_MAX
};
struct page_list{
                   u32 type;
                   u32 size;
                   u32 free_pages;
                   u32 last_free_pos;
                   u64 start_address;
                   void *bitmap;
};


struct osalloc_chunk{
                  u16 chunk_size;
                  u16 free_position;
                  u32 current_pfn;
                  struct list freelist;
                  char bitmap[16];   /*current page bitmap*/
}; 

struct osalloc_nugget_meta{
    u32 region;
    u16 nugget_size[7]; //sizes 32,64,...,2048
    struct list_head size_list[7];
    struct list_head list;
};

struct osalloc_nugget{
    u8 is_full;
    u16 chunk_size;
    u64 current_pfn;
    char bitmap[16]; //bit value 1 means free
    struct list_head list;
}; 

#define NODE_MEM_PAGES 100

struct nodealloc_memory{
                  u32 next_free;
                  u32 num_free;
                  void *nodes;
                  void *end;
};

static inline int get_mem_region(u32 pfn)
{
    u64 address = (u64) pfn << PAGE_SHIFT;
    if(address < REGION_OS_DS_START || address > REGION_NVM_ENDMEM) 
         return -1;
    else if (address >= REGION_OS_DS_START && address < REGION_OS_PT_START)
          return OS_DS_REG;
    else if (address >= REGION_OS_PT_START && address < REGION_USER_START)
           return OS_PT_REG;
    else if (address >= REGION_USER_START && address < REGION_FILE_DS_START)
           return USER_REG;
    else if (address >= REGION_FILE_DS_START && address < REGION_FILE_STORE_START)
           return FILE_DS_REG;
    else if (address >= REGION_FILE_STORE_START && address < ENDMEM)
           return FILE_STORE_REG;
    else if (address >= NVM_META_REG_START && address < NVM_USER_REG_MAGIC)
           return NVM_META_REG;
    else if (address >= NVM_USER_REG_MAGIC && address < NVM_USER_REG_START)
           return NVM_MAGIC_REG;
    else if (address >= NVM_USER_REG_START && address < REGION_NVM_ENDMEM)
           return NVM_USER_REG;

   return -1 ;   
}


extern void init_mems();
extern void* os_alloc(u32 size, u32 region);
extern void os_free(void *, u32, u32 region);
extern void* os_page_alloc(u32);
extern void* os_page_alloc_pages(u32,u32);
extern u32 os_pfn_alloc(u32);
extern u32 os_pfns_alloc(u32,u32);
extern void os_pfn_free(u32, u64);
extern void os_pfns_free(u32, u64, u32);
extern void os_page_free(u32, void *);
extern void os_pages_free(u32, void *,u32);
extern struct node *node_alloc(void);
extern void node_free(struct node *);
extern int get_free_pages_region(u32);
extern int get_pages_region(u32);
extern u64 os_chunk_alloc(u32 size, u32 region);
extern u8 os_chunk_free( u32 region, u64 address, u32 size);
void *osmap(u64);
#endif
