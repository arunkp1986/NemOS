#include<ulib.h>

#define LLC_SIZE (1<<21)
#define NUM_ACCESS 100
#define PAGE_SIZE 4096
#define TRACK_SIZE 64

unsigned lfsr_rand(unsigned seed){
    unsigned lfsr = seed;
    u16 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}

void merging(u32*a, int low, int mid, int high){
    int l1, l2, i;
    u32 num_elements = LLC_SIZE/sizeof(int);
    u32 b[num_elements]; 
    for(l1=low, l2=mid+1, i=low; l1<=mid && l2<=high; i++){
        if(a[l1] <= a[l2]){
            b[i] = a[l1++];
	}
	else{
	    b[i] = a[l2++];
	}
    }
    while(l1<=mid){
        b[i++] = a[l1++];
    }
    while(l2<=high){
        b[i++] = a[l2++];
    } 
    for(int i=low; i<=high; i++){
        a[i] = b[i];
    } 
    return;
}

void sort(u32* a, int low, int high){
    int mid = (low+high)/2;
    if(low<high){
    sort(a,low,mid);
    sort(a,mid+1,high);
    merging(a, low, mid, high);
    }
    return;
}
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5){
    //u32 num_elements = LLC_SIZE/sizeof(int);
    u32 num_elements = 100;
    u32 seed = 0xACE1u;
    u32 rand_value = 0;
    u32* input;
    input = (u32*)mmap(NULL,num_elements*sizeof(u32),PROT_READ|PROT_WRITE,0);
    for(int i=0; i<num_elements; i++){
        rand_value = rand_value ? lfsr_rand(rand_value) : lfsr_rand(seed);
	input[i] = rand_value;
    }
    int* temp = (int*)mmap(NULL,2*LLC_SIZE*sizeof(u32),PROT_READ|PROT_WRITE,0);
    umemset(temp, 0, 2*LLC_SIZE);
    checkpoint_start(1000);
    sort(input,0,num_elements-1);
    checkpoint_end();
    return 0;
}
