/*
	low level functions to allocate deallocate pages of memory
*/

#include <types.h>
#include <string.h>
#include "assert.h"
#include "kernel.h"
#include "mem.h"
#include "isr.h"
#include "multiboot.h"
#include "console.h"
#include "kheap.h"

#define KERNEL_DIR_UP	0xFFFFF000	// recursive page dir addr

void *ikheap = 0;					// pointer used in base mem alloc

unsigned int num_frames;			// number of frames
unsigned int *frames_stack;			// will keep a stack of frames
unsigned int stack_idx;				// index to the frame_stack
unsigned int up_mem;				// upper memory in bytes
unsigned int *kernel_dir = NULL;	// main kernel page directory


//
//	Pushes a free frame address into stack
//
void push_frame(unsigned int frame_addr) {
	KASSERT((frame_addr & 0xFFFFF000) | (frame_addr == 0));
	frames_stack[stack_idx++] = frame_addr;
}

//
//	Pops a free frame address from stack
//
unsigned int pop_frame(){
	if(stack_idx == 0) {
		panic("Out of physical memory!");
	}
	stack_idx--;
	KASSERT(stack_idx != num_frames);
	unsigned int frame_addr = frames_stack[stack_idx];
	frames_stack[stack_idx] = 0;
	return frame_addr;
}

unsigned int remove_frame(unsigned int frame_addr){
	unsigned int i, found;
	for(i=0, found=0; i < stack_idx; i++) {
		if(frames_stack[i] == frame_addr) {
			found=1;
			stack_idx--;
		}
		if(found) {
			frames_stack[i] = frames_stack[i+1];
		}
	}
	if(!found) {
		panic("In remove_frame: cannot find 0x%X frame\n", frame_addr);
	}
	return found ? 1:0;
}

void page_fault(struct iregs *r) {
	unsigned int fault_addr;
	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));
	kprintf("Page fault! ( ");
	kprintf("%s", (r->err_code & 0x1) ? "page-level violation ": "not-present ");
	kprintf("%s", (r->err_code & 0x2) ? "write ":"read ");
	kprintf("%s", (r->err_code & 0x4) ? "user-mode ":"supervisor-mode ");
	kprintf(") at 0x%X\n", fault_addr);
	
	return;
}
void map_linear_to_physical(unsigned int *dir, unsigned int linear_addr, unsigned int physical_addr, unsigned int flags) {
	unsigned short int t_idx = (linear_addr & 0xFFC00000) >> 22;
	unsigned short int p_idx = (linear_addr & 0x3FF000) >> 12;
	unsigned int *table;
	unsigned int t;
	if (dir[t_idx] & 0x1) {
		table = (unsigned int *) (dir[t_idx] & 0xFFFFF000);
		table[p_idx] = physical_addr | 3;
	} else {
		t = pop_frame();
		dir[t_idx] = t | flags;
		map_linear_to_physical(dir, t, t, flags);
		table = (unsigned int *) t;
		table[p_idx] = physical_addr | 3;
	}
}
void mem_init(multiboot_header *mboot) 
{
	unsigned int i;

	ikheap = (void *) kinfo.end;

	// setup frames stack //
	i = (((mboot->mem_upper/1024) + 2)*1024*1024) / PAGE_SIZE;	// aproximate size of frames_stack
	frames_stack = ikheap;
	ikheap += i * sizeof(frames_stack); // make room for frame stack //

	// if boot loader passed a mmap structure //
	if(testb(mboot->flags, MBOOTF_MMAP)) 
	{
		memory_map* mmap;
		mmap = (memory_map *) mboot->mmap_addr;
		do {
			if(mmap->type == 1) {
				unsigned int base = (mmap->base_addr_high << 16) + mmap->base_addr_low;
				unsigned int length = (mmap->length_high << 16) + mmap->length_low;
				for(i = base; i < length; i += PAGE_SIZE, num_frames++) {
					push_frame(i);
				}
			}
			mmap = (memory_map *) ((unsigned long) mmap + mmap->size + sizeof(mmap->size));
		}
		while ((unsigned long) mmap < mboot->mmap_addr + mboot->mmap_length);
	} else { 
		// aproximate the memory //
		for(i = 0; i < 640*1024; i += PAGE_SIZE, num_frames++) {
			push_frame(i);
		}
		unsigned int up_mem = mboot->mem_upper * 1024;
		for(i = 0x100000; i < up_mem; i += PAGE_SIZE, num_frames++) {
			push_frame(i);
		}
	}
	
	// remove kernel pages from stack //
	for(i = kinfo.code; i < (unsigned int)ikheap; i+=PAGE_SIZE) {
		remove_frame(i);
	}
	
	kernel_dir = (unsigned int *) pop_frame();
	memset(kernel_dir, '\0', PAGE_SIZE);
	map_linear_to_physical(kernel_dir, (unsigned int)kernel_dir, (unsigned int)kernel_dir, 3);


	
	for(i=0xB8000; i < (unsigned int)ikheap; i+=PAGE_SIZE ) {
		map_linear_to_physical(kernel_dir, i, i, 3);
	}

	// init heap //
//	map_range_any(kernel_dir, KHEAP_START, KHEAP_START+KHEAP_INIT_SIZE, P_PRESENT | P_READ_WRITE);
	for(i = KHEAP_START; i < KHEAP_START+KHEAP_INIT_SIZE; i+= PAGE_SIZE) {
		map_linear_to_physical(kernel_dir, i, pop_frame(), 3);
	}


	switch_page_directory(kernel_dir);

	unsigned int *x = (unsigned int *) 0xDEADC000;
	map_linear_to_physical(kernel_dir, (unsigned int)x, pop_frame(), 3);
	flush_tlb();
	x[0] = 0xDEADC0DE;
	kprintf("0x%x\n", x[0]);

	unsigned int *arr[1000];
	for(i=1; i < 1000; i++) {
			arr[i] = kalloc(5120); // 5K
	}
	kprintf("--0x%X\n", arr[20]);
	for(i=999; i > 0; i--) {
			kfree(arr[i]);
	}
	kheap_dump();
}