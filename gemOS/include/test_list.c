#include "list.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define is_set(x, pos)    (((x) >> (pos)) & 0x1)

typedef unsigned u32;
typedef unsigned long long u64;

struct list_head mylist;
//LIST_HEAD(mylist);
struct node_32{
    unsigned num1;
    unsigned num2;
    unsigned num3;
    struct list_head list;
};
struct node_64{
    unsigned num1;
    unsigned num2;
    unsigned num3;
    char padding[32];
    struct list_head list;
};
struct node_128{
    unsigned num1;
    unsigned num2;
    unsigned num3;
    char padding[64];
    struct list_head list;
};

struct node_256{
    unsigned num1;
    unsigned num2;
    unsigned num3;
    char padding[128];
    struct list_head list;
};

struct osalloc_chunk_new{
    short is_full;
    unsigned chunk_size;
    u64 current_pfn;
    char bitmap[16];
    struct list_head list;
}; 


LIST_HEAD(os_chunk_alloc_list);

void* os_page_alloc(u32 region){
    void * ptr = mmap(NULL, 4096, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED){
        printf("mmap failed\n");
	return NULL;
    }
    return ptr;
}

#define PAGE_SHIFT 12
#define PAGE_SIZE 4096
#define FLAG_MASK 0x3ffffffff000UL 


unsigned long long os_pfn_alloc(u32 region){
    void * ptr = mmap(NULL, 4096, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED){
        printf("mmap failed\n");
	return 0;
    }
    return ((u64)ptr>>PAGE_SHIFT);
}


void init_os_chunk_alloc(u32 region){
    struct osalloc_chunk_new * ch_new = (struct osalloc_chunk_new*)os_page_alloc(region);
    ch_new->is_full = 0;
    ch_new->chunk_size = 32;
    ch_new->current_pfn = os_pfn_alloc(region);
    for(int i=0; i<4; i++){
        *((unsigned int*)ch_new->bitmap+i) |= ~(1UL<<32);
    }
    //ch_new->bitmap |= ~(1UL<<(sizeof(ch_new->bitmap)*8));
    list_add_tail(&ch_new->list,&os_chunk_alloc_list);
}


struct osalloc_chunk_new * get_new_os_chunk(u32 size, u32 region){
    struct list_head* temp = os_chunk_alloc_list.next;
    int i = 0;
    int st_size = sizeof(struct osalloc_chunk_new);
    
    while(st_size>>1 >0){
        i +=1;
	st_size = st_size>>1;
    }
    
    if((1 << i) != size)
        ++i;

    u32 num_chunks = PAGE_SIZE>>i;
    printf("num chunks in get: %u\n",num_chunks);
    struct osalloc_chunk_new * ch_new;
    u32 pos = 1;
    while(temp->next != &os_chunk_alloc_list){
        pos +=1;
	temp = temp->next;
    }
    if(pos%num_chunks != 0){
	ch_new = (struct osalloc_chunk_new*)((u64)list_entry(temp, struct osalloc_chunk_new, list)+\
			sizeof(struct osalloc_chunk_new));
        ch_new->is_full = 0;
        ch_new->chunk_size = size;
        ch_new->current_pfn = os_pfn_alloc(region);
	for(int i=0; i<4; i++){
            *((unsigned int*)ch_new->bitmap+i) |= ~(1UL<<32);
	}
        //ch_new->bitmap |= ~(1UL<<(sizeof(ch_new->bitmap)*8));
        list_add_tail(&ch_new->list,&os_chunk_alloc_list);
    }
    else{
        ch_new = (struct osalloc_chunk_new*)os_page_alloc(region);
	if(!ch_new)
            return NULL;

	ch_new->is_full = 0;
        ch_new->chunk_size = size;
        ch_new->current_pfn = os_pfn_alloc(region);
	for(int i=0; i<4; i++){
            *((unsigned int*)ch_new->bitmap+i) |= ~(1UL<<32);
	}
        //ch_new->bitmap |= ~(1UL<<(sizeof(ch_new->bitmap)*8));
        list_add_tail(&ch_new->list,&os_chunk_alloc_list);
    }
    return ch_new;
}

u64 os_chunk_alloc(u32 size, u32 region){
    struct osalloc_chunk_new *os_ch;
    struct list_head* temp;
    short found = 0;
    u64 retval;
    
    int i = 0;
    if(!size || size > 2048)
       return 0;

    for(i=11; i>5; --i){
     if(is_set(size, i))
          break;
    }
    if((1 << i) != size)
        ++i;

    u32 num_bits = PAGE_SIZE>>i;

    list_for_each(temp,&os_chunk_alloc_list){
        os_ch = list_entry(temp, struct osalloc_chunk_new, list);
        if(os_ch->chunk_size == size){
            if(!os_ch->is_full){
		int loop_counter = 0;
                while( loop_counter < num_bits){
		    unsigned int * bitmap_loc = (unsigned int*)os_ch->bitmap+(loop_counter>>5); 
                    if( *bitmap_loc & 1UL<<(31-(loop_counter%(1<<5)))){
			*bitmap_loc &= ~(1UL<<(31-(loop_counter%(1<<5))));
		        retval = (u64)(os_ch->current_pfn<<PAGE_SHIFT)+(loop_counter*size);
			return retval;
		    }
		    loop_counter += 1;
		}
                os_ch->is_full = 1;
	    }
	}
    }
    os_ch = get_new_os_chunk(size, region);
    if(!os_ch)
        return 0;
    if(!os_ch->is_full){
        int loop_counter = 0;
        while( loop_counter < num_bits){
            unsigned int* bitmap_loc = (unsigned int*)os_ch->bitmap+(loop_counter>>5);
            if( *bitmap_loc & 1UL<<(31-(loop_counter%(1<<5)))){
                 *bitmap_loc &= ~(1UL<<(31-(loop_counter%(1<<5))));
		retval = (u64)(os_ch->current_pfn<<PAGE_SHIFT)+(loop_counter*size);
		return retval;
	    }
            loop_counter += 1;
	}
	os_ch->is_full = 1;
    }
    else{
        printf("chunk alloc Bug!!, new allocated node should not be full\n");
    }
    return 0;
}

short os_chunk_free(u64 address, u32 size){
    u64 pfn = address>>PAGE_SHIFT;
    struct osalloc_chunk_new *os_ch;
    struct list_head* temp;

    int i = 0;
    if(!size || size > 2048)
       return 0;

    for(i=11; i>5; --i){
     if(is_set(size, i))
          break;
    }
    if((1 << i) != size)
        ++i;

    u32 num_bits = PAGE_SIZE>>i;

    list_for_each(temp,&os_chunk_alloc_list){
        os_ch = list_entry(temp, struct osalloc_chunk_new, list);
        if(os_ch->current_pfn == pfn){
            u32 pos = (address&~FLAG_MASK)>>i;
	    unsigned int * bitmap_loc = (unsigned int*)os_ch->bitmap+(pos>>5);
	    if( *bitmap_loc & (1UL<< (31-pos%(1<<5)))){
	        printf("trying to free %u which is already marked free\n",(127-pos));
	    }
	    *bitmap_loc |= (1UL<<(31-pos%(1<<5)));
	    return 1;
	}
    }
    return 0;
}

int main(){
    init_os_chunk_alloc(0);
    printf("size:%lu\n",sizeof(struct osalloc_chunk_new));
    for(int i=0; i<100; i++){
    struct node_32 * n1 = (struct node_32*)os_chunk_alloc(sizeof(struct node_32),0);
    n1->num1 = 0;
    n1->num2 = 1;
    n1->num3 = 2;
    //printf("num1: %u, num2: %u, num3: %u\n",n1->num1,n1->num2,n1->num3);
    struct node_64 * n2 = (struct node_64*)os_chunk_alloc(sizeof(struct node_64),0);
    n2->num1 = 0;
    n2->num2 = 1;
    n2->num3 = 2;
    //printf("num1: %u, num2: %u, num3: %u\n",n2->num1,n2->num2,n2->num3);
    struct node_128 * n3 = (struct node_128*)os_chunk_alloc(sizeof(struct node_128),0);
    n3->num1 = 0;
    n3->num2 = 1;
    n3->num3 = 2;
    //printf("num1: %u, num2: %u, num3: %u\n",n3->num1,n3->num2,n3->num3);
    struct node_256 * n4 = (struct node_256*)os_chunk_alloc(sizeof(struct node_256),0);
    n4->num1 = 0;
    n4->num2 = 1;
    n4->num3 = 2;
    //printf("num1: %u, num2: %u, num3: %u\n",n4->num1,n4->num2,n4->num3);
    }
    return 0;
}

/*
int main(){
    init_list_head(&mylist);
    struct node n1,n2,n3,n4;
    struct list_head *temp;
    struct node *temp_node;
    n1.num = 0;
    list_add_tail(&n1.list,&mylist);
    n2.num = 1;
    list_add_tail(&n2.list,&mylist);
    n3.num = 2;
    list_add_tail(&n3.list,&mylist);
    n4.num = 3;
    list_add_tail(&n4.list,&mylist);
    list_for_each(temp,&mylist){
        temp_node = list_entry(temp, struct node, list);
        printf("%d\n",temp_node->num);
    }
    struct list_head *next;
    temp = mylist.next;
    next = temp;
    int count = 0;
    while( next != &mylist){
        next = temp->next;
	temp->next = NULL;
	temp->prev = NULL;
	temp = next;
	count +=1;
    }
    printf("count:%d\n",count);
    return 0;
}*/
