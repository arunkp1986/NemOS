#ifndef __NV_MEMORY_H_
#define __NV_MEMORY_H_

#include <list.h>
#include <context.h>

#define MAKE_PERSISTENT 0x1
#define FIRST_TIME 0x2

struct ssp_entry{
   unsigned long p0;
   unsigned long p1;
   unsigned long current_bitmap;
   unsigned long updated_bitmap;
   unsigned evicted;
};

struct ssp_commit_entry{
   u64 p0;
   u64 p1;
   u64 commit_bitmap;
};

struct ssp_bitmap_entry{
    struct ssp_entry *entry;
    struct ssp_commit_entry *commit_entry;
    struct list_head list;
};

extern struct list_head* saved_state_list;
enum{
  BEGIN_LOG,
  CTX_CHANGE,
  VMA_CHANGE,
  VMA_ADD,
  VMA_REMOVE,
  END_LOG,
};

/*if saved_state state is MODIFYING, then execution context
 * is getting updated with logs, reapply changes from logs 
 * if state is MODIFYING after a crash.
 * if state is UPDATED, then all changes are applied and latest
 * filed points to index of updated execution context*/
enum{
  BEGIN_STATE,
  INVALID,
  INITIAL,
  MAPPING,
  MODIFYING,
  UPDATED,
  END_STATE,
};

struct mapping_table {
  u64 vaddr;
  u64 nvaddr;
  u64 access_flags;
  struct mapping_table* next;
};

struct mapping_entry {
  u64 vaddr;
  u64 nvaddr;
  u64 access_flags;
  struct list_head list;
};
/*
struct stack_entry {
  u64 addr;
  u64 size;
  void* payload;
  struct list_head list;
};*/

struct stack_entry {
  u64 addr;
  u64 size;
  struct list_head list;
  union{
  char byte[0];
  char *page;
  }payload;
};



struct vaddr_list {
  u64 vaddr;
  // int pid;
  struct vaddr_list* next;
};
/*
struct log_entry {
  int event;
  void* payload;
  struct log_entry *next;
};
*/
struct log_entry{
  int event;
  struct {
  u16 index;
  u16 offset;
  }address;
  void* payload;
  u64 size;
  struct list_head list;
};

/*This is per process saved state
 * One of the execution context is the latest one
 * nv_mappings: It contains the list of VA->NVPA mapping for stack used pages
 * log: The redo log has execution context changes
 * nv_stack: persisted copy of stack changes
 * */
struct saved_state {
  int state;
  int pid;
  int latest;
  struct exec_context *context[2];
  struct list_head nv_mappings; //for stack, VA->NVPA mapping
  struct list_head log; // for ctx and vma changes
  struct list_head nv_stack; //having stack modification
  struct list_head list;
};


extern struct saved_state *create_saved_state(struct exec_context *ctx);
extern struct saved_state *get_saved_state(struct exec_context *ctx);
extern void make_nv_mappings(struct exec_context *ctx, u64 vaddr, u32 access_flags);
extern u8 is_mapping_present(struct saved_state *cs, u64 vaddr);
extern struct mapping_entry * get_nv_mapping(struct saved_state *cs, u64 vaddr);
extern void init_saved_state();
extern void init_saved_state_list();
extern  void clflush_multiline(u64 address, u32 size); 
#endif
