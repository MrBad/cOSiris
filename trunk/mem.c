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

/*
		--------------------------------- < 0
		|								|
		| 								|					low memory, unmapped by default, managed by frames stack,
		| 		LOW physical memory		|
		|								| 
		|-------------------------------- < 640K
		| 		UNUSED					|					special zone, unused (BIOS?!), should not be reported by grub
		|-------------------------------| < 0xB8000
		|		VIDEO memory			|					video memory, text mode, identity mapped
		|-------------------------------| < 0xC8000
		| 		UNUSED					|					special zone, (graphic video memory, BIOS EBDA), should not be reported by grub 
		|								|
		|-------------------------------| < 0x100000 == kinfo.code
		|								|
		|		KERNEL					|					kernel memory, identity mapped
		|								|
		|-------------------------------| < kinfo.end
		|		FRAMES STACK			|					frames stack, identity mapped - will manage physical memory
		|-------------------------------| < ikheap
		|								|
		.		REST OF PHYS MEM		.					rest of phyisical mem, unmapped by default, managed	by frames stack
		|								|
		--------------------------------- < end of physical mem
		|								|
		|								|
		|		HOLE					|
		|								|
		--------------------------------- < KHEAP_START					kheap expandable mem allocator, with KHEAP_INIT_SIZE 
		|		KHEAP					|								initial size, malloc, free 
		|	 ------------------------	| < KHEAP_START+KHEAP_INIT_SIZE
		|								|
		--------------------------------| < KHEAP_END == 0xEFFFF000
		|								|
		|								|
		|		HOLE					|
		|								|
		--------------------------------- < KERNEL_TBL_MAP,		1024 PDEs, each containing 1024 PTEs, each mapping 4MB of linear addr 
		|		Kernel pages			|						4MB, allocated on the fly, mapping the whole mem			
		|								|
		|	 ------------------------	| < KERNEL_DIR_MAP
		|		kernel_dir map			|						mapped to the kernel dir
		--------------------------------- < 0xFFFFFFFF == 4GB
		

*/

#define PDE_IDX(x) ( x >> 22 )
#define PTE_IDX(x) ( (x >> 12) & 0x03FF )

enum {
	KERNEL_DIR_MAP	= 0xFFFFF000,	// recursive page dir addr
	KERNEL_TBL_MAP	= 0xFFC00000,	
	PAGE_MASK		= 0xFFFFF000,
};


void *ikheap = 0;					// pointer used in base mem alloc

unsigned int num_frames;			// number of frames
unsigned int *frames_stack;			// will keep a stack of frames
unsigned int stack_idx;				// index to the frame_stack
unsigned int up_mem;				// upper memory in bytes
unsigned int paging_enabled = 0;	// is paging enabled?
unsigned int *kernel_dir = NULL;	// main kernel page directory
unsigned int *current_dir = NULL;	// current page directory

//
//	Pushes a free frame address into stack
//
void push_frame(unsigned int frame_addr) {
	KASSERT((frame_addr & PAGE_MASK) | (frame_addr == 0));
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

//
//	Removes a frame from stack
//
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

//
//	Called by int 14
//
void page_fault(struct iregs *r) {
	unsigned int fault_addr;

	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));
	
	
	// page is not present //
	if(! (r->err_code & 0x1)) {
		unsigned int pde_idx = PDE_IDX(fault_addr);
		unsigned int pte_idx = PTE_IDX(fault_addr);
		
		if(fault_addr >= KERNEL_TBL_MAP) {
			kprintf("vmm needs pages pde:%i, pte:%i\n", pde_idx, pte_idx);
		/*	unsigned int *dir = (unsigned int *)KERNEL_DIR_MAP;
			if(! dir[pde_idx]) {
				panic("needs another table\n");
			} else {
				unsigned int *t = (unsigned int *) KERNEL_TBL_MAP + pde_idx * 1024;
				t[pte_idx] = pop_frame() | 3;
				return;
			}*/
		}
	} 
	
	kprintf("Page fault! ( ");
	kprintf("%s", (r->err_code & 0x1) ? "page-level violation ": "not-present ");
	kprintf("%s", (r->err_code & 0x2) ? "write ":"read ");
	kprintf("%s", (r->err_code & 0x4) ? "user-mode ":"supervisor-mode ");
	kprintf(") at 0x%X, eip:0x%x\n", fault_addr, r->eip);
	

	panic("\n");
}

//
//	Maps a virtual to physical address
//
void map_linear_to_physical(unsigned int linear_addr, unsigned int physical_addr, unsigned int flags) {
	unsigned int *table, frame;
	unsigned int pde_idx = PDE_IDX(linear_addr);
	unsigned int pte_idx = PTE_IDX(linear_addr);
	

	if( paging_enabled) {
		current_dir = (unsigned int * ) KERNEL_DIR_MAP;
	}

	if(! (current_dir[pde_idx] & P_PRESENT)) {
		frame = pop_frame();
		current_dir[pde_idx] = frame | P_PRESENT | P_READ_WRITE;
	}
		
	if(paging_enabled) {
		table = (unsigned int *) (KERNEL_TBL_MAP) + 1024 * pde_idx;
	} else {
		table = (unsigned int *) (current_dir[pde_idx] & PAGE_MASK);
	}

	if(table[pte_idx] & P_PRESENT) {
		panic("linear_addr 0x%x allready mapped, %x, %x!\n", linear_addr, table, table[pte_idx]);
	}
	table[pte_idx] = physical_addr | flags;
}

//
//	Returns the physical addr where virtual_addr is mapped
//
unsigned int get_physical_addr(unsigned int virtual_addr) {
	unsigned short int pde_idx = PDE_IDX(virtual_addr);
	unsigned short int pte_idx = PTE_IDX(virtual_addr);
	unsigned int *table;
	unsigned int *dir;
	
	if(! paging_enabled) {
		dir = current_dir;
		if(! (dir[pde_idx] & P_PRESENT)) {
			panic("%x is not mapped\n", virtual_addr);
		}
		table = (unsigned int *) (dir[pde_idx] & PAGE_MASK);
		return (table[pte_idx] & PAGE_MASK);
	} else {
		table = (unsigned int *)KERNEL_TBL_MAP + pde_idx*1024 + pte_idx;
		return (*table & PAGE_MASK);
	}
}

//
//	Dumps a page directory
//
void dump_vmm() {
	unsigned int i/*, j*/;
	unsigned int *table;
	if(! paging_enabled) {
		kprintf("Paging disabled, kernel_dir @phys 0x%x\n", kernel_dir);
		for(i=0; i<1024; i++) {
			if(kernel_dir[i] & P_PRESENT) {
				kprintf("pde: %i, table addr: 0x%x\n", i, (kernel_dir[i] & PAGE_MASK));
				table = (unsigned int *) (kernel_dir[i] & PAGE_MASK);
//				for(j=0; j < 1024; j++) {
//					if(table[j])
//					kprintf("\tpte: %i, phys: %i\n", j, table[j] & PAGE_MASK);
//				}
			}
		}
	} else {
		kprintf("Paging enabled, kernel_dir @phys 0x%x\n", get_physical_addr(KERNEL_DIR_MAP));
		unsigned int *dir = (unsigned int *) KERNEL_DIR_MAP;
		for(i=0; i<1024; i++) {
			if(dir[i] & P_PRESENT) {
				kprintf("pde: %i, table addr: 0x%x\n", i, dir[i] & PAGE_MASK);
				table = (unsigned int *) (KERNEL_TBL_MAP + 1024 * i);
//				for(j=0; j< 1024; j++) {
					//kprintf("{%i, %i} ", i, j);
					//if(table[j])
					//	kprintf("\tpte: %i, phys: %i\n", j, table[j] & PAGE_MASK);
//				}
			}
		}
	}
}

//
//	Initialize the vmm and heap
//
void mem_init(multiboot_header *mboot) 
{
	unsigned int i;

	paging_enabled = 0;
	ikheap = (void *) kinfo.end;

	// setup frames stack //
	i = (((mboot->mem_upper/1024) + 2)*1024*1024) / PAGE_SIZE;	// aproximate size of frames_stack
	frames_stack = ikheap;
	ikheap += i * sizeof(frames_stack); // make room for frame stack //

	unsigned int up_mem = mboot->mem_upper * 1024;
	
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
		for(i = 0x100000; i < up_mem; i += PAGE_SIZE, num_frames++) {
			push_frame(i);
		}
	}
	

	// remove kernel pages from stack, we don't want these pages to be allocated, because now kernel is identity mapped //
	for(i = kinfo.code; i < (unsigned int)ikheap; i+=PAGE_SIZE) {
		remove_frame(i);
	}
	
	// define the kernel page directory //
	kernel_dir = (unsigned int *) pop_frame();
	kernel_dir[1023] = (unsigned int) kernel_dir|3; // self mapping to the last 4MB of virtual space
	current_dir = kernel_dir;

	// identity map video memory //
	for(i=0xB8000; i < 0xC0000; i+=PAGE_SIZE ) {
		map_linear_to_physical(i, i, P_PRESENT|P_READ_WRITE);
	}
	
	// identity map all pages from start of the kernel, to the end of ikheap //
	for(i = kinfo.code; i < (unsigned int) ikheap; i+=PAGE_SIZE ) {
		map_linear_to_physical(i, i, P_PRESENT|P_READ_WRITE);
	}
	
	// map initial kernel heap //
	for(i = KHEAP_START; i < KHEAP_START+KHEAP_INIT_SIZE; i+= PAGE_SIZE) {
		map_linear_to_physical(i, pop_frame(), P_PRESENT|P_READ_WRITE);
	}

	// enable paging //
	switch_page_directory(kernel_dir);
	paging_enabled = 1;
	kprintf("%u total physical pages, %u allocated\n", num_frames, num_frames-stack_idx-1);

	kprintf("Testing memory.\n");

	// write @ 0xDEADC000, 0xDEADCODE
	unsigned int *x = (unsigned int *) 0xDEADC000;
	map_linear_to_physical((unsigned int)x, pop_frame(), P_PRESENT|P_READ_WRITE);
	flush_tlb();
	x[0] = 0xDEADC0DE;
	kprintf("0x%x\n", x[0]);

	// malloc 4000 buffers of 5k //
	unsigned int *arr[1000];
	for(i=1; i < 1000; i++) {
		arr[i] = kalloc(5120); // 5K
	}
	
	// and free them //
	for(i=999; i > 0; i--) {
		kfree(arr[i]);
	}
	kheap_dump();
	dump_vmm(kernel_dir);
	kprintf("end vmm\n");
}