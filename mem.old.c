#include "include/types.h"
#include "include/string.h"
#include "console.h"
#include "isr.h"
#include "multiboot.h"
#include "assert.h"
#include "mem.h"
#include "kheap.h"
#include "x86.h"


#define CURRENT_PAGE_DIRECTORY   0xFFFFF000ul
#define CURRENT_PAGE_TABLE_BASE  0xFFC00000ul

#define KHEAP_START	0xC0000000
#define KHEAP_END 	0xD0000000

typedef unsigned int phys_t;
typedef unsigned int virt_t;

unsigned int num_frames = 0;
unsigned int *frames_stack;
unsigned int stack_idx = 0;

void *kmalloc_ptr = 0;
void *kmalloc_max = 0;
bool paging_on = false;
unsigned int *kernel_dir = NULL;
unsigned int *current_dir = NULL;
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
	kprintf("____\nFAULT: %s addr: %p, ", r->err_code & P_PRESENT ? "page violation" : "not present", fault_addr);
	kprintf("stack: 0x%X\n", get_esp());
	kprintf("frame_stack index: 0x%i\n", stack_idx);
	kprintf("vaddr: %p, dir_idx: %i, table_idx: %i\n", table_idx*1024*PAGE_SIZE+page_idx*PAGE_SIZE, table_idx, page_idx);
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
void *kmalloc(unsigned int size, int align, unsigned *phys)
{
	void *tmp = 0;
	KASSERT(kmalloc_ptr != NULL);
	KASSERT(kmalloc_max != NULL);
	// if align and not allready aligned
	if (align && ((unsigned int) kmalloc_ptr & 0xFFF)) {
		kmalloc_ptr = (void *) ((unsigned int) kmalloc_ptr & 0xFFFFF000) + PAGE_SIZE;
	}
	KASSERT(kmalloc_ptr + size < kmalloc_max);
	tmp = kmalloc_ptr;
	if (phys) {
		*phys = (unsigned) tmp;
	}
	kmalloc_ptr += size;
	if(paging_on) {
		unsigned int frame = pop_frame();
		map((unsigned int)tmp, frame, P_PRESENT|P_READ_WRITE);
		if(phys) {
			*phys = frame;
		}
	}
	return tmp;
}

void *page_ptr = NULL;
void *alloc_page(unsigned *phys)
{
	void *tmp;
	unsigned int frame;
	KASSERT((unsigned int)page_ptr < KHEAP_END);
	if(!paging_on) {
		return NULL;
	}
	if(page_ptr == NULL) {
		page_ptr = (void *)KHEAP_START;
	}
	tmp = page_ptr;
	frame = pop_frame();
	map((unsigned int)tmp, frame, P_PRESENT|P_READ_WRITE);
	if(phys) {
		*phys = frame;
	}
	page_ptr += PAGE_SIZE;
	return tmp;
}
void free_page(unsigned int *page)
{
	// kprintf("is mapped: %p - %c\n", page, is_mapped((unsigned int) page) ? 'Y':'N');
	// kprintf("Free page %p\n", page);
}

/**
 * Checks if this virtual address is mapped anywhere
 */
int is_mapped(unsigned int virtual_addr) {

	unsigned int *table = NULL;
	unsigned short int dir_idx, table_idx;
	int is_present;
	// kprintf("{%p} \n", virtual_addr);
	dir_idx = virtual_addr >> 22;
	table_idx = virtual_addr >> 12 & 0x03FF;
	if (!(current_dir[dir_idx] & P_PRESENT)) {
		return false;
	}
	// kprintf("{%p} direxist \n", virtual_addr);
	table = (unsigned int *) (current_dir[dir_idx] & 0xFFFFF000); // mask flags bits
	// kprintf("{%p} after table \n", table);
	// if(paging_on) {
	// 	map((unsigned int) reserved_page, (unsigned int)table, P_PRESENT | P_READ_WRITE);
	// 	is_present = reserved_page[table_idx] & P_PRESENT;
	// 	unmap((unsigned int) reserved_page);
	// } else {
		is_present = table[table_idx] & P_PRESENT;
	// }
	return is_present;

	// if (!(table[table_idx] & P_PRESENT)) {
		// return false;
	// }
	// return true;
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
	table = (unsigned int *) (current_dir[dir_idx] & 0xFFFFF000); // mask flags bits
	return (table[table_idx] & ~0xFFF) + (virtual_addr & 0xFFF);
}

void unmap(unsigned int virtual_addr) {
	unsigned short int dir_idx, table_idx;
	unsigned int *table = NULL;
	unsigned int all_empty, i;

	dir_idx = virtual_addr >> 22;
	table_idx = virtual_addr >> 12 & 0x03FF;
	KASSERT(is_mapped(virtual_addr));

	table = (unsigned int *) (current_dir[dir_idx] & 0xFFFFF000); // mask flags bits
	table[table_idx] = 0;

	all_empty = true;
	for (i = 0; i < 1024; i++) {
		if (table[i] != 0) {
			all_empty = false;
			break;
		}
	}
	if (all_empty) {
		current_dir[dir_idx] = 0;
	}
	if (virtual_addr != (unsigned int) reserved_page) {
		push_frame(virtual_addr);
	}
	invlpg(virtual_addr);
}

virt_t *temp_map(phys_t addr)
{
	map((unsigned int)reserved_page, addr, P_PRESENT | P_READ_WRITE);
	return reserved_page;
}
void temp_unmap()
{
	unmap((unsigned int)reserved_page);
}

/**
 * Maps a virtual address to physical one, using flags
 */
void map(unsigned int virtual_addr, unsigned int physical_addr, unsigned short int flags) {
	unsigned int *table = NULL;
	unsigned int frame = 0;
	unsigned short int dir_idx, table_idx;
	// dir_idx = virtual_addr >> 22;
	// table_idx = virtual_addr >> 12 & 0x03FF;
	dir_idx = virtual_addr / PAGE_SIZE / 1024;
	table_idx = (virtual_addr / PAGE_SIZE) % 1024;

	if (!current_dir[dir_idx] & P_PRESENT) {
		if (paging_on) {
			kprintf("Making table for dir_idx: %i, %s\n", dir_idx, paging_on ? "paging ON" : "paging OFF");
			frame = pop_frame();
			map((unsigned int) reserved_page, frame, P_PRESENT | P_READ_WRITE);
			memset((void *) reserved_page, 0, PAGE_SIZE);
			unmap((unsigned int) reserved_page);
			invlpg(dir_idx*PAGE_SIZE*1024);
		}
		else {
			kmalloc(PAGE_SIZE, true, &frame); // will be mapped in mem_init()
//			frame = pop_frame();
//			map(frame, frame, P_READ_WRITE | P_PRESENT);
			memset((void *) frame, 0, PAGE_SIZE);
		}
		current_dir[dir_idx] = frame | P_PRESENT | P_READ_WRITE;
	}
	table = (unsigned int *) (current_dir[dir_idx] & 0xFFFFF000); // mask flags bits
	if (paging_on && !is_mapped((unsigned int) table)) {
		map((unsigned int) reserved_page, (unsigned int) table, P_PRESENT | P_READ_WRITE);
		reserved_page[table_idx] = physical_addr | flags;
		unmap((unsigned int) reserved_page);
		// flush_tlb();
	}
	else {
		table[table_idx] = physical_addr | flags;
	}
	invlpg(virtual_addr);
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

void dump_current_dir()
{
	unsigned int i, j;
	unsigned int *table, block;
	for(i = 0; i < 1024; i++) {
		if(current_dir[i] & P_PRESENT) {
			table = (unsigned int *) (current_dir[i] & 0xFFFFF000);
			kprintf("dir_idx: %d, is %s\n", i, is_mapped((unsigned int)table) ? "present":"not present");
			for(j=0; j< 1024; j++) {
				if(table[j] & P_PRESENT) {
					block =  (table[j] & 0xFFFFF000);
					// virtual -> mapped to -> physical //
					kprintf("%08X-%08X ", i * PAGE_SIZE * 1024 + j * PAGE_SIZE, block);
				}
			}
			kprintf("\n");
		}
	}
}

unsigned int *clone_current_dir()
{
	unsigned int i, j;
	unsigned int *new_dir;
	unsigned int frame;
	unsigned int *src_table, *dest_table;

	new_dir = (unsigned int *) pop_frame();
	kprintf("selfmap\n");
	map((unsigned int)new_dir,(unsigned int)new_dir,P_PRESENT|P_READ_WRITE);
	kprintf("new_dir at: %p, reserved_page: %p\n", new_dir, reserved_page);
	map((unsigned int)reserved_page, (unsigned int)new_dir, P_PRESENT | P_READ_WRITE);
	memset((void*) reserved_page, 0, PAGE_SIZE);
	unmap((unsigned int)reserved_page);

	for(i = 0; i < 1024; i++) {
		if(current_dir[i] & P_PRESENT) {

			if(!(current_dir[i] & P_USER)) {
				map((unsigned int)reserved_page, (unsigned int)new_dir, P_PRESENT | P_READ_WRITE);
				reserved_page[i] = current_dir[i]; // is kernel, just link
				unmap((unsigned int)reserved_page);
			} else {
				// user space // clone
				frame = pop_frame();
				map((unsigned int) reserved_page, frame, P_PRESENT | P_READ_WRITE);
				memset((void *) reserved_page, 0, PAGE_SIZE);
				unmap((unsigned int) reserved_page);

				map((unsigned int)reserved_page, (unsigned int)new_dir, P_PRESENT | P_READ_WRITE);
				reserved_page[i] = frame | P_PRESENT | P_READ_WRITE | P_USER;
				unmap((unsigned int)reserved_page);

				src_table = (unsigned int *) (current_dir[i] & 0xFFFFF000);
				for(j = 0 ; j < 1024; j++) {
					if(src_table[j] & P_PRESENT) {

						frame = pop_frame();
						map((unsigned int) reserved_page, frame, P_PRESENT | P_READ_WRITE);
						memcpy((void *)reserved_page, (void *) (i*PAGE_SIZE*1024+j*PAGE_SIZE), PAGE_SIZE);
						unmap((unsigned int) reserved_page);

						map((unsigned int)reserved_page, (unsigned int)new_dir, P_PRESENT | P_READ_WRITE);
						dest_table = (unsigned int *)(reserved_page[i] & 0xFFFFF000);
						unmap((unsigned int)reserved_page);

						map((unsigned int)reserved_page, (unsigned int)dest_table, P_PRESENT | P_READ_WRITE);
						reserved_page[j] = frame | P_PRESENT | P_READ_WRITE | P_USER;
						unmap((unsigned int)reserved_page);
					}
				}
			}
		}
	}

	// unsigned int is_present, *table;
	// for(i=0; i< 1024; i++) {
	// 	map((unsigned int) reserved_page, (unsigned int) new_dir, P_PRESENT | P_READ_WRITE);
	// 	table = (unsigned int *)(reserved_page[i] & 0xFFFFF000);
	// 	is_present = reserved_page[i] & P_PRESENT;
	// 	unmap((unsigned int) reserved_page);
	// 	if(is_present) {
	// 		map((unsigned int) reserved_page, (unsigned int)table, P_PRESENT | P_READ_WRITE);
	// 		for(j=0; j<1024; j++) {
	// 			is_present = reserved_page[j] & P_PRESENT;
	// 			if(is_present) {
	// 				kprintf("%p-%p %c\n ", i*1024*PAGE_SIZE+j*PAGE_SIZE, reserved_page[j]&0xFFFFF000, reserved_page[j]&P_USER ? 'u':'k');
	// 			}
	// 		}
	// 		unmap((unsigned int) reserved_page);
	// 	}
	// }
	return new_dir;
}


void kernel_identity_map()
{

}

void mem_init(multiboot_header *mboot, unsigned int *free_mem_ptr)
{
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

	kernel_dir = (unsigned int *) kmalloc(PAGE_SIZE, true, NULL);
	memset(kernel_dir, 0, PAGE_SIZE);
	current_dir = kernel_dir;

	num_frames = (mem - (unsigned int) &end) / PAGE_SIZE;
	frames_stack = kmalloc(num_frames * sizeof(int), true, NULL);
	memset(frames_stack, 0, num_frames * sizeof(int));

	kmalloc_ptr = (unsigned int *) (((unsigned int) kmalloc_ptr & 0xFFFFF000) + PAGE_SIZE); // align
	KASSERT((unsigned) kmalloc_ptr % PAGE_SIZE == 0);

	for (i = (unsigned int) kmalloc_ptr; i < mem; i += PAGE_SIZE) {
		push_frame(i);
	}
	for (i = HEAP_START; i < HEAP_START + HEAP_INITIAL_SIZE; i += PAGE_SIZE) {
		map(i, pop_frame(), P_PRESENT | P_READ_WRITE | P_USER);
	}
	// map all alocated space, this allways should come last,
	// because kmalloc_ptr grows when paging is off //
	for (i = 0xB8000; i < (unsigned int) kmalloc_ptr; i += PAGE_SIZE) {
		map(i, i, P_PRESENT | P_READ_WRITE);
	}

	heap->start_addr = HEAP_START;
	heap->end_addr = HEAP_START + HEAP_INITIAL_SIZE;
	heap->max_addr = HEAP_END;
	heap->readonly = false;
	heap->supervisor = true;

	current_dir[1023] = (unsigned int)current_dir | P_PRESENT | P_READ_WRITE; // recursive map //

	switch_page_directory(current_dir);
	paging_on = true;

	// kmalloc_max = kmalloc_ptr; // invalidate kmalloc //
	// kmalloc_ptr = (unsigned int *)KHEAP_START;
	// kmalloc_max = (unsigned int *)KHEAP_END;
	//
	// unsigned int phys, *ptr;
	// ptr = alloc_page(&phys);
	// kprintf("%p, %p\n", ptr, phys);
	// memset(ptr, 0, PAGE_SIZE);

	// kprintf("is mapped: %p - %c\n", ptr, is_mapped((unsigned int)ptr) ? 'Y':'N');

	// free_page(ptr);
	// for(i=0;i<100;i++) {
	// 	ptr = alloc_page(&phys);
	// 	memset(ptr, 0, PAGE_SIZE);
	// }
	// kprintf("%p, %p\n", ptr, phys);
	dump_current_dir();
}
