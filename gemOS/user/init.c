/*
 *Here stream access to stack sets all bits in dirty bitmap
 *This test case is to check scalability of the dirty tracking scheme
 *
 * */
#include<ulib.h>

#define PAGE_SIZE 4096
#define SIZE (8*PAGE_SIZE)
#define TARCK_SIZE 64

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{  
   char * ptr1 = (char*)mmap(NULL,4096,PROT_WRITE,MAP_NVM);
   ptr1[0] = 'A';
   printf("ptr1:%x, value:%c\n",ptr1,ptr1[0]);
   char * ptr2 = (char*)mmap(NULL,4096,PROT_WRITE,0);
   ptr2[0] = 'B';
   printf("ptr2:%x, value:%c\n",ptr2,ptr2[0]);
   munmap(ptr1, 4096);
   munmap(ptr2, 4096);
   return 0;
}
