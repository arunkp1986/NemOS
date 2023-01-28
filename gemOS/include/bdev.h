#ifndef __BDEV_H__
#define __BDEV_H__
#include<types.h>

// PCI query and manipulation macros

   #define PCI_CONFIG_ADDRESS 0xCF8
   #define PCI_CONFIG_DATA 0xCFC

   struct pci_dev{
	   u32 base_address;
	   u64 bars[6];
	   u8 irq;
   };

#if 0
  #define mmio_write(type, addr, x) do{ \
	                             void *inaddr = (void *)addr;\
	                             *((type *)inaddr) = x; \
	                             asm volatile("mfence;" \
				         	  "clflush %0;" \
					          :             \
					          :"m" (inaddr) \
					          :"memory"); \
	                   }while(0);
  
 #define mmio_read(type, addr, x) do{ \
	                             void *inaddr = (void *)addr;\
	                             asm volatile("mfence;" \
				         	  "clflush %0;" \
					          :             \
					          :"m" (inaddr) \
					          :"memory"); \
	                              x = *((type *)inaddr); \
	                   }while(0);
#endif

// IDE device pio interfaces

   #define BDEV_BASE 0x1F0
   #define OFF_DATA 0
   #define OFF_ERROR_FEAT 1 
   #define SEC_COUNT 2
   #define SEC_NUM_LOW 3
   #define SEC_NUM_MID 4
   #define SEC_NUM_HIGH 5
   #define DRV_HEAD 6
   #define STAT_CMD 7

   #define BDEV_CTRL_BASE 0x3F6
   #define DEV_CTRL 2
   #define DEV_ADDR 3

   #define SECTOR_SIZE 512
   #define SECTOR_BITS 9
   #define NUM_SEC_PER_BLOCK 8
   
   #define BDEV_CMD_READ  0x20
   #define BDEV_CMD_WRITE 0x30
   #define BDEV_CMD_RDMUL 0xC4
   #define BDEV_CMD_WRMUL 0xC5
   #define BDEV_CMD_FLUSH 0xE7

   #define SEL_DISK0_CMD 0xE0
   #define SEL_DISK1_CMD 0xF0

   #define BDEV_BUSY       0x80
   #define BDEV_READY      0x40
   #define BDEV_DF        0x20
   #define BDEV_DR        0x8
   #define BDEV_ERR       0x1

  // PCI IDE controller interfaces
  
   #define IDE_VENDOR_ID 0x8086 
   #define IDE_DEVICE_ID 0x7111

   extern int bdev_read_block(char *buf, u32 block_offset);
   extern int bdev_write_block(char *buf, u32 block_offset);
   extern int bdev_init(void);

#endif
