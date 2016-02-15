#include "include/types.h"
#include "include/string.h"
#include "console.h"
#include "isr.h"
#include "multiboot.h"
#include "assert.h"
#include "mem.h"
#include "kheap.h"
#include "x86.h"

extern void switch_page_directory(unsigned int *dir);


extern void flush_tlb();

page_directory_t *kernel_dir = 0;

unsigned int num_frames = 0;
unsigned int *frames_stack;
unsigned int stack_idx = 0;

void *kmalloc_ptr = 0;
void *kmalloc_max = 0;
bool heap_active = false;
bool switched = false;


void debug_print_directory(page_directory_t *dir) {
	int i, j;
	for (i = 0; i < 1024; i++) {
		if (!dir->tables[i]) {
			continue;
		}
		kprintf("%.4i: 0x%X, 0x%X\n", i, dir->tables[i], dir->tablesPhysical[i]);
		for (j = 0; j < 1024; j++) {
			page_t page = dir->tables[i]->pages[j];
			if (page.frame == 0) {
				continue;
			}
//			kprintf("\t%.4i: pt: 0x%X\n", j, dir->tables[i]->pages[j]);
		}
	}
}


void push_frame(unsigned int frame_addr) {
	KASSERT((frame_addr & 0xFFFFF000) | (frame_addr == 0));
	frames_stack[stack_idx++] = frame_addr;
}

unsigned int pop_frame() {
	stack_idx--;
	KASSERT(stack_idx != num_frames);
	if (stack_idx == 0) {
		panic("Out of memory in pop_frame\n");
	}
	unsigned int frame_addr = frames_stack[stack_idx];
	KASSERT(frame_addr != 0);
	frames_stack[stack_idx] = 0;
	return frame_addr;
}

void remove_frame(unsigned int frame_addr) {
	unsigned int i, found;
	for (i = 0, found = 0; i < stack_idx; i++) {
		if (frames_stack[i] == frame_addr) {
			found = 1;
			stack_idx--;
		}
		if (found) {
			frames_stack[i] = frames_stack[i + 1];
		}
	}
	if (!found) {
		panic("In remove_frame: cannot find 0x%X frame\n", frame_addr);
	}
}


static inline void invlpg(unsigned int addr) {
	asm volatile("invlpg (%0)"::"r" (addr) : "memory");
}

int count = 0;

int page_fault(struct iregs *r) {
	unsigned int fault_addr;
	unsigned int phys;
	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));
	unsigned short int table_idx = fault_addr >> 22;
	unsigned short int page_idx = fault_addr >> 12 & 0x03FF;

	count++;
	if (count > 10) {
		panic("Too many faults, addr: 0x%X\n", fault_addr);
	}

	if (! (r->err_code & P_PRESENT)) {
		phys = pop_frame();
		kprintf("Mapping %X to %X, tidx: %i, pidx: %i\n", fault_addr, phys, table_idx, page_idx);
//		map(kernel_dir, fault_addr, phys);
//		flush_tlb();
//		invlpg((fault_addr & ~(0xFFF)));
//		return false;
	}

	kprintf("Page fault! ( ");
	kprintf("%s", (r->err_code & 0x1) ? "page-level violation " : "not-present ");
	kprintf("%s", (r->err_code & 0x2) ? "write " : "read ");
	kprintf("%s", (r->err_code & 0x4) ? "user-mode " : "supervisor-mode ");
	kprintf(") at 0x%X\n", fault_addr);
	kprintf("HeapAt: 0x%X\n", heap->end_addr);
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

unsigned short int table_index_of(unsigned int addr) {
	unsigned short int table_idx = addr >> 22; // div 4M
	return table_idx;
}

unsigned short int page_index_of(unsigned int addr) {
	unsigned short int page_idx = addr >> 12 & 0x03FF; // div 1024
	return page_idx;
}


void map(page_directory_t *dir, unsigned int virt_addr, unsigned int phys_addr) {
	unsigned short int table_idx = table_index_of(virt_addr);
	unsigned short int page_idx = page_index_of(virt_addr);
	unsigned int phys;
	page_t *page;

	if (!dir->tablesPhysical[table_idx]) {
		kprintf("table_idx: %i,  page_idx: %i, switched: %s\n", table_idx, page_idx, switched ? "yes" : "no");
		phys = pop_frame();
		dir->tables[table_idx] = (page_table_t *) phys;
		map(dir, phys, phys); // self mapping identity this page
		dir->tablesPhysical[table_idx] = phys | P_PRESENT | P_READ_WRITE | P_CACHE_DISABLED;
	}

	page = &dir->tables[table_idx]->pages[page_idx];
	page->present = true;
	page->writable = true;
	page->frame = virt_addr >> 12;

}

void unmap(page_directory_t *dir, unsigned virt_addr);

unsigned int *dir = NULL;

int is_mapped(unsigned int virtual_addr) {
	unsigned int *table = NULL;
	unsigned short int dir_idx, table_idx;
	dir_idx = virtual_addr / PAGE_SIZE / 1024;
	table_idx = (virtual_addr % (PAGE_SIZE * 1024)) / PAGE_SIZE;
	if(! (dir[dir_idx] & P_PRESENT)) {
		return false;
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
	if(!(table[table_idx] & P_PRESENT)) {
		return false;
	}
	return true;
}

unsigned int get_phys_addr(unsigned int virtual_addr) {
	unsigned int *table = NULL;
	unsigned short int dir_idx, table_idx;
	dir_idx = virtual_addr / PAGE_SIZE / 1024;
	table_idx = (virtual_addr % (PAGE_SIZE * 1024)) / PAGE_SIZE;
	if(!is_mapped(virtual_addr)) {
		panic("NOT MAPPED %X\n", virtual_addr);
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
//	kprintf("---%i, %i, 0x%X\n", dir_idx, table_idx, (table[table_idx] &~0xFFF)+(virtual_addr&0xFFF));
//	halt();
	return (table[table_idx] &~0xFFF)+(virtual_addr&0xFFF);
	return 0;
}


void map_address(unsigned int virtual_addr, unsigned int physical_addr, unsigned short int flags) {
	unsigned int *table = NULL;
	unsigned int frame = 0;
	unsigned short int dir_idx, table_idx;
	dir_idx = virtual_addr / PAGE_SIZE / 1024;
	table_idx = (virtual_addr % (PAGE_SIZE * 1024)) / PAGE_SIZE;
	if (!dir[dir_idx] & P_PRESENT) {
		kprintf("Making table for dir_idx: %i\n", dir_idx);
		kmalloc(PAGE_SIZE, true, &frame);
		KASSERT(is_mapped(frame));
		memset((void *) frame, 0, PAGE_SIZE);
		dir[dir_idx] = frame | P_PRESENT | P_READ_WRITE;
	}

	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000); // mask flags bits
	table[table_idx] = physical_addr | flags;
}


void mem_init(multiboot_header *mboot) {
	extern unsigned int end; // generated by linker - ldlink.ld
	unsigned int mem, i; // maximum available memory

	mem = (unsigned int) 0x100000 + (mboot->mem_upper) * 1024 + 0x10000;
	kprintf("MEM: 0x%X\n", mem);
	kmalloc_ptr = (void *) &end;
	kmalloc_max = (void *) mem;

	dir = kmalloc(PAGE_SIZE, true, NULL);
	memset(dir, 0, PAGE_SIZE);

	unsigned int frame;
	for (i = 0; i < mem; i += PAGE_SIZE * 1024) {
		kmalloc(PAGE_SIZE, true, &frame);
		memset((void *) frame, 0, PAGE_SIZE);
		dir[table_index_of(i)] = frame | P_PRESENT | P_READ_WRITE;
	}
	for (i = HEAP_START; i < HEAP_END; i += PAGE_SIZE * 1024) {
		kmalloc(PAGE_SIZE, true, &frame);
		memset((void *) frame, 0, PAGE_SIZE);
		dir[table_index_of(i)] = frame | P_PRESENT | P_READ_WRITE;
	}

	for (i = 0xB8000; i < (unsigned int) kmalloc_ptr; i += PAGE_SIZE) {
		map_address(i, i, P_PRESENT | P_READ_WRITE);
	}

	switch_page_directory(dir);
	switched = true;

	for (i = HEAP_START; i < HEAP_START + HEAP_INITIAL_SIZE; i += PAGE_SIZE) {
		kmalloc(PAGE_SIZE, true, frame);
		map_address(i, frame, P_PRESENT | P_READ_WRITE | P_USER);
	}

	kprintf("zero\n");
	unsigned int k = HEAP_START; int l=0;
	for (i = (unsigned int) kmalloc_ptr; i < kmalloc_max; i++) {
		if(!is_mapped(i)) {
			l++;
//			kprintf("Not mapped, stealing frame for %X\n", i);
//			kprintf("%s free - 0x%X\n",is_mapped(k)?"Mapped":"Not mapped", get_phys_addr(k));
			map_address(i, get_phys_addr(k), P_PRESENT|P_READ_WRITE);
			k+=PAGE_SIZE;
			if(l > 10) {
//				halt();
			}
		}
		char *p = (char *) i;
		*p = 0;
	}
	kprintf("Zeroed\n");
//	kprintf("zero heap\n");
//	for (i = HEAP_START; i < HEAP_START+HEAP_INITIAL_SIZE; i++) {
//		char *p = (char *) i;
//		*p = 0;
//	}
//	kprintf("Zeroed heap\n");

}