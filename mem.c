#include "include/types.h"
#include "include/string.h"
#include "console.h"
#include "isr.h"
#include "multiboot.h"
#include "assert.h"
#include "mem.h"
#include "kheap.h"
#include "x86.h"


unsigned int num_frames = 0;
unsigned int *frames_stack;
unsigned int stack_idx = 0;

void *kmalloc_ptr = 0;
void *kmalloc_max = 0;
bool paging_on = false;
unsigned int *dir = NULL;
unsigned int *reserved_page = (unsigned int *) (PAGE_SIZE * 1023);
unsigned int fault_counter = 0;


void push_frame(unsigned int frame_addr) {
	KASSERT(frame_addr & 0xFFFFF000);
	KASSERT(frame_addr != 0);
	frames_stack[stack_idx++] = frame_addr;
}

unsigned int pop_frame() {
	stack_idx--;
//	KASSERT(stack_idx != num_frames);
	if (stack_idx == 1) {
		panic("Out of memory in pop_frame\n");
	}
	unsigned int frame_addr = frames_stack[stack_idx];
	KASSERT(frame_addr != 0);
	frames_stack[stack_idx] = 0;
	return frame_addr;
}
//
//void remove_frame(unsigned int frame_addr) {
//	unsigned int i, found;
//	for (i = 0, found = 0; i < stack_idx; i++) {
//		if (frames_stack[i] == frame_addr) {
//			found = 1;
//			stack_idx--;
//		}
//		if (found) {
//			frames_stack[i] = frames_stack[i + 1];
//		}
//	}
//	if (!found) {
//		panic("In remove_frame: cannot find 0x%X frame\n", frame_addr);
//	}
//}


static inline void invlpg(unsigned int addr) {
	asm volatile("invlpg (%0)"::"r" (addr) : "memory");
}


/**
 * Page fault isr
 */
int page_fault(struct iregs *r) {
	unsigned int fault_addr;
	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));
	unsigned short int table_idx = fault_addr >> 22;
	unsigned short int page_idx = fault_addr >> 12 & 0x03FF;
	extern unsigned int get_esp();
	fault_counter++;
	if (fault_counter > 10) {
		halt("Too many faults, addr: 0x%X\n", fault_addr);
	}
	kprintf("____\nFAULT: %s 0x%X, ", r->err_code & P_PRESENT ? "page violation" : "not present", fault_addr);
	kprintf("stack: 0x%X\n", get_esp());
	kprintf("frame_stack index: 0x%i\n", stack_idx);
	kprintf("dir_idx: %i, table_idx: %i\n", table_idx, page_idx);
	kprintf("%s, ", r->err_code & P_READ_WRITE ? "write" : "read", fault_addr);
	kprintf("%s\n", r->err_code & P_USER ? "user-mode" : "kernel", fault_addr);
	kprintf("%s", is_mapped(fault_addr) ? "mapped" : "not mapped");
	if (HEAP_START < fault_addr && fault_addr < HEAP_END) {
		kprintf(", in heap\n");
	}
	kprintf("\n%s", paging_on ? "paging ON" : "paging OFF");
//	panic("\npanic\n____");
	return false;
}


/**
 * kernel dumb placement allocator
 */
void *kmalloc(unsigned int size, int align, unsigned *phys) {
	void *tmp = 0;
	KASSERT(kmalloc_ptr != NULL);
	KASSERT(kmalloc_max != NULL);
	// if align and not allready aligned
	if (align && ((unsigned int) kmalloc_ptr & 0xFFF)) {
		kmalloc_ptr = (void *) ((unsigned int) kmalloc_ptr & 0xFFFFF000) + 0x1000;
	}
	KASSERT(kmalloc_ptr + size < kmalloc_max);
	tmp = kmalloc_ptr;
	if (phys) {
		*phys = (unsigned) tmp;
	}
	kmalloc_ptr += size;
	return tmp;
}

/**
 * Checks if this virtual address is mapped anywhere
 */
int is_mapped(unsigned int virtual_addr) {
	unsigned int *table = NULL;
	unsigned short int dir_idx, table_idx;
	dir_idx = virtual_addr / PAGE_SIZE / 1024;
	table_idx = (virtual_addr % (PAGE_SIZE * 1024)) / PAGE_SIZE;
	if (!(dir[dir_idx] & P_PRESENT)) {
		return false;
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
	if (!(table[table_idx] & P_PRESENT)) {
		return false;
	}
	return true;
}

/**
 * Gets physical address of the virtual one
 */
unsigned int get_phys_addr(unsigned int virtual_addr) {
	unsigned int *table = NULL;
	unsigned short int dir_idx, table_idx;
	dir_idx = virtual_addr / PAGE_SIZE / 1024;
	table_idx = (virtual_addr % (PAGE_SIZE * 1024)) / PAGE_SIZE;
	if (!is_mapped(virtual_addr)) {
		panic("NOT MAPPED %X\n", virtual_addr);
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
	return (table[table_idx] & ~0xFFF) + (virtual_addr & 0xFFF);
}

void unmap(unsigned int virtual_addr) {
	unsigned short int dir_idx, table_idx;
	unsigned int *table = NULL;
	unsigned int all_empty, i;

	dir_idx = virtual_addr >> 22;
	table_idx = virtual_addr >> 12 & 0x03FF;
	KASSERT(is_mapped(virtual_addr));

	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
	table[table_idx] = 0;

	all_empty = true;
	for (i = 0; i < 1024; i++) {
		if (table[i] != 0) {
			all_empty = false;
			break;
		}
	}
	if (all_empty) {
		dir[dir_idx] = 0;
	}
	if (virtual_addr != (unsigned int) reserved_page) {
		push_frame(virtual_addr);
	}
}

/**
 * Maps a virtual address to physical one, using flags
 */
void map(unsigned int virtual_addr, unsigned int physical_addr, unsigned short int flags) {
	unsigned int *table = NULL;
	unsigned int frame = 0;
	unsigned short int dir_idx, table_idx;
	dir_idx = virtual_addr >> 22;
	table_idx = virtual_addr >> 12 & 0x03FF;

	if (!dir[dir_idx] & P_PRESENT) {
		if (paging_on) {
			kprintf("Making table for dir_idx: %i, %s\n", dir_idx, paging_on ? "paging ON" : "paging OFF");
			frame = pop_frame();
			map((unsigned int) reserved_page, frame, P_PRESENT | P_READ_WRITE);
			memset((void *) reserved_page, 0, PAGE_SIZE);
			unmap((unsigned int) reserved_page);
		}
		else {
			kmalloc(PAGE_SIZE, true, &frame);
//			frame = pop_frame();
//			map(frame, frame, P_READ_WRITE | P_PRESENT);
//			kprintf("->%X\n", frame);
			memset((void *) frame, 0, PAGE_SIZE);
		}
		dir[dir_idx] = frame | P_PRESENT | P_READ_WRITE;
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
	if (paging_on && !is_mapped((unsigned int) table)) {
		map((unsigned int) reserved_page, (unsigned int) table, P_PRESENT | P_READ_WRITE);
		reserved_page[table_idx] = physical_addr | flags;
		unmap((unsigned int) reserved_page);
	}
	else {
		table[table_idx] = physical_addr | flags;
	}
}

void *sbrk(unsigned int increment) {
	unsigned int start = heap->end_addr;
	unsigned int frame, i;
	unsigned int iterations = increment / PAGE_SIZE;
	KASSERT(heap);

	if (iterations < 1) {
		iterations++;
	}
	for (i = 0; i < iterations; ++i) {
		frame = pop_frame();
		map(heap->end_addr, frame, P_PRESENT | P_READ_WRITE);
		heap->end_addr += PAGE_SIZE;
		if (heap->end_addr % PAGE_SIZE > 0) {
			panic("---%X\n", frame);
		}
		if (heap->end_addr > heap->max_addr) {
			panic("Out of memory on sbrk: heap end addr: 0x%X, size: %iM\n", heap->end_addr,
			      (heap->end_addr - heap->start_addr) / 1024 / 1024);
		}
	}
	return (void *) start;
}

void mem_init(multiboot_header *mboot, unsigned int *free_mem_ptr) {
	extern unsigned int end; // generated by linker - ldlink.ld
	unsigned int mem; // maximum available memory
	unsigned int num_frames, i;


	mem = (unsigned int) 0x100000 + (mboot->mem_upper) * 1024;
	kprintf("MEM: 0x%X\n", mem);
	if(free_mem_ptr) {
		kmalloc_ptr = (void *)free_mem_ptr;
	}
	else {
		kmalloc_ptr = (void *) &end;
	}
	kmalloc_max = (void *) mem;


	dir = (unsigned int *) kmalloc(PAGE_SIZE, true, NULL);
	memset(dir, 0, PAGE_SIZE);

	num_frames = (mem - (unsigned int) &end) / PAGE_SIZE;
	frames_stack = kmalloc(num_frames * sizeof(int), true, NULL);
	memset(frames_stack, 0, num_frames * sizeof(int));

	kmalloc_ptr = (unsigned int *) (((unsigned int) kmalloc_ptr & 0xFFFFF000) + PAGE_SIZE); // align
	KASSERT((unsigned) kmalloc_ptr % PAGE_SIZE == 0);

	for (i = (unsigned int) kmalloc_ptr; i < mem; i += PAGE_SIZE) {
		push_frame(i);
	}
	for (i = 0xB8000; i < (unsigned int) kmalloc_ptr; i += PAGE_SIZE) {
		map(i, i, P_PRESENT | P_READ_WRITE);
	}
	for (i = HEAP_START; i < HEAP_START + HEAP_INITIAL_SIZE; i += PAGE_SIZE) {
		map(i, pop_frame(), P_PRESENT | P_READ_WRITE | P_USER);
	}

	heap->start_addr = HEAP_START;
	heap->end_addr = HEAP_START + HEAP_INITIAL_SIZE;
	heap->max_addr = HEAP_END;
	heap->readonly = false;
	heap->supervisor = true;
	switch_page_directory(dir);
	paging_on = true;

	kmalloc_max = kmalloc_ptr; // invalidate kmalloc //
//	kprintf("%X\n", kmalloc_ptr);

//	kprintf("zero heap\n");
//	for (i = HEAP_START; i < HEAP_START + HEAP_INITIAL_SIZE; i++) {
//		char *p = (char *) i;
//		*p = 0;
//	}
//	kprintf("Zeroed heap\n");
}