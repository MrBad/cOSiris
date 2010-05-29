#include "multiboot.h"
#include "console.h"


#define MBOOT_MAGIC 0x2BADB002

#define MBOOTF_MEM		0	// multiboot memory flag
#define MBOOTF_BOOTD	1	// multiboot boot device flag
#define MBOOTF_ELF		5	// 
#define MBOOTF_MMAP		6	// memory map struct

static unsigned short int boot_device = 0;

void multiboot_parse(unsigned int magic, multiboot_header *mboot) {
//	kprintf("0x%X\n", magic);
	if(magic != MBOOT_MAGIC) {
		panic("Invalid magic flag: 0x%X\n", magic);
	}
	kprintf("Multiboot flags: 0x%X\n", mboot->flags);
	if(testb(mboot->flags, MBOOTF_MEM)) {
		kprintf("Low memory: %iKb, upper memory:%iKb\n", mboot->mem_lower, mboot->mem_upper);
	}
	if(testb(mboot->flags, MBOOTF_BOOTD)) {
		boot_device = (unsigned short int) (mboot->boot_device & 0xFF000000) >> 12;
		kprintf("Boot device:0x%X\n", boot_device);
	}
	if(testb(mboot->flags, MBOOTF_MMAP)) {
		kprintf("Detecting memory:\n");
		memory_map *mmap;
		mmap = (memory_map *) mboot->mmap_addr;
		do {
			kprintf ("\tbase_addr = 0x%x%x, length = 0x%x%x, type = 0x%x\n",
				(unsigned) mmap->base_addr_high, (unsigned) mmap->base_addr_low,
				(unsigned) mmap->length_high, (unsigned) mmap->length_low,
				(unsigned) mmap->type);
			mmap = (memory_map *) ((unsigned long) mmap + mmap->size + sizeof(mmap->size));
		} while((unsigned long) mmap < mboot->mmap_addr + mboot->mmap_length);
	}
}