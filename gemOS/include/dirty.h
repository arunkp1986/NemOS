#ifndef DIRTY_HEADER_F
#define DIRTY_HEADER_F

#include <context.h>
#include <list.h>

/* if check pointing is based on time interval, then set 
 * TRACK_ON and CHECKPOINT_INTERVAL to the required value
 * and TARCK_SIZE is fixed to 64 Bytes 
 * if check pointing is not based on time interval, then reset
 * TRACK_ON and CHECKPOINT_INTERVAL is fixed to 10 ms and
 * TARCK_SIZE is varied based on requirement.
 * */

#define TRACK_ON 0
#define TRACK_BYTE_GRAN 1
#define CPU_CLOCK 3000000000 //3GHz
#define ONE_MICRO_SEC 3000
#define ONE_MILLI_SEC 3000000
#define CHECKPOINT_INTERVAL (ONE_MILLI_SEC*5) //rdtsc cycles corresponding to x milli seconds
#define TRACK_SIZE 0

//#define LOG_TRACK_GRAN 6 //64 bytes

struct checkpoint_stats{
    u32 num;
    u64 time_to_copy;
    u64 time_to_log;
    u32 bytes_copy;
    struct list_head list;
};

extern struct list_head* checkpoint_stat_list; 
extern u32 check_point;
extern u16 t_size;
extern void start_track(struct exec_context * ctx, u16 track_size);
extern void end_track(struct exec_context * ctx);
extern u32 read_dirty_bitmap(struct exec_context *ctx, u64 start, u64 end, u16 track_size);
extern void alloc_se_dirty_bitmap(struct exec_context *ctx, u64 start, u64 end, u16 track_size);
extern void custom_print(u64 temp);
extern void clear_dirtybit(struct exec_context *ctx, u64 start, u64 end);
extern u32 read_dirtybit(struct exec_context *ctx, u64 start, u64 end);
extern void alloc_se_dirtybit(struct exec_context *ctx, u64 start, u64 end);
extern int resume_persistent_processes(); 
extern unsigned print_checkpoint_stats();
#endif
