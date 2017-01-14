#include <types.h>		// types //
#include "string.h"		// memset //
#include "console.h"	// kprint, panic//
#include "assert.h"		// KASSERT /
#include "isr.h" 		// iregs //
#include "multiboot.h"	// multiboot_header //
#include "kernel.h" 	// kinfo_t //
#include "kheap.h"		// HEAP defs //
#include "mem.h"

// extern void halt(void);



phys_t	last_free_page = 0;
size_t	num_pages = 0;
dir_t	*kernel_dir = NULL;

#define RESV_PAGE		0xFFBFE000ul
#define PTABLES_ADDR 	0xFFC00000ul
#define PDIR_ADDR		0xFFFFF000ul

bool is_mapped(virt_t addr);

static bool pg_on = false;
bool paging_on()
{
	return pg_on;
}


/**
 * Page fault isr
 */
int page_fault(struct iregs *r)
{
	static int fault_counter = 0;
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
	kprintf("eip: 0x%p\n", r->eip);
	kprintf("vaddr: %p, dir_idx: %i, table_idx: %i\n", table_idx*1024*PAGE_SIZE+page_idx*PAGE_SIZE, table_idx, page_idx);
	kprintf("%s, ", r->err_code & P_READ_WRITE ? "write" : "read", fault_addr);
	kprintf("%s\n", r->err_code & P_USER ? "user-mode" : "kernel", fault_addr);
	kprintf("%s", is_mapped(fault_addr) ? "mapped" : "not mapped");
	if (HEAP_START < fault_addr && fault_addr < HEAP_END) {
		kprintf(", in heap\n");
	}
	kprintf("\n%s", paging_on() ? "paging ON" : "paging OFF");
//	panic("\npanic\n____");
	return false;
}

//
//	----------
//	ptr->next pointer to next free page
//	page 4096-4
//  .........
//
phys_t *page_alloc()
{
	phys_t *addr;
	KASSERT(last_free_page != 0);
	addr = (phys_t *)last_free_page;
	if(paging_on()) {
		map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
		last_free_page = *((phys_t *)RESV_PAGE);
		*((phys_t *)RESV_PAGE) = 0;
		unmap(RESV_PAGE);
		//kprintf("Alloc %p\n", addr);
	} else {
		last_free_page = *addr;
		*addr = 0; // unlink next //
	}
	num_pages--;
	return addr;
}

// allocs and bzero page //
phys_t *page_calloc()
{
	phys_t *addr;
	addr = page_alloc();
	if(paging_on()) {
		map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
		memset((void *)RESV_PAGE, 0, PAGE_SIZE);
		unmap(RESV_PAGE);
	} else {
		memset(addr, 0, PAGE_SIZE);
	}
	return addr;
}

void page_free(phys_t addr)
{
	KASSERT(!(addr & 0xFFF));
	// kprintf("%p, %p, nump: %d\n", addr, last_free_page, num_pages);
	if(paging_on()){
		map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
		*((phys_t *)RESV_PAGE) = last_free_page;
		unmap(RESV_PAGE);
		kprintf("freep %p\n", addr);
	}else {
		*((phys_t *)addr) = last_free_page;
	}
	last_free_page = addr;
	num_pages++;
}

static void reserve_region(phys_t addr, size_t length, phys_t kernel_end_addr)
{
	struct kinfo_t kinfo;
	phys_t ptr;

	get_kernel_info(&kinfo);

	for(ptr = addr; ptr - addr < length; ptr+=PAGE_SIZE) {
		if(ptr == 0) {
			*((phys_t *) addr) = 0;
			continue;
		}
		if(ptr >= VGA_FB_ADDR && ptr <= kernel_end_addr) {
			continue;
		}
		page_free(ptr);
	}
}

static void reserve_memory(multiboot_header *mb, phys_t kernel_end_addr)
{
	memory_map *mm;
	mm = (memory_map *) mb->mmap_addr;
	do {
		if(mm->type == 1) {
			phys_t addr;
			size_t length;
			addr = (mm->base_addr_high << 16) + mm->base_addr_low;
			length = (mm->length_high << 16) + mm->length_low;
			reserve_region(addr, length, kernel_end_addr);
		}
        mm = (memory_map *) ((unsigned long) mm + mm->size + sizeof(mm->size));
    } while((unsigned long) mm < mb->mmap_addr + mb->mmap_length);
}

static void recursively_map_page_directory(dir_t *dir)
{
	dir[1023] = (phys_t) dir | P_PRESENT | P_READ_WRITE;
}

static void identity_map_kernel(dir_t *dir, phys_t kernel_end_addr)
{
	struct kinfo_t kinfo;
	phys_t addr, *table;
	int dir_idx, tbl_idx;

	get_kernel_info(&kinfo);

	for(addr = VGA_FB_ADDR; addr <= kernel_end_addr; addr += PAGE_SIZE) {
		dir_idx = (addr / PAGE_SIZE) / 1024;
		tbl_idx = (addr / PAGE_SIZE) % 1024;
		if(!(dir[dir_idx] & P_PRESENT)) {
			dir[dir_idx] = (uint32_t) page_calloc() | P_PRESENT | P_READ_WRITE;
		}
		table = (phys_t *)(dir[dir_idx] & 0xFFFFF000);
		table[tbl_idx] = addr | P_PRESENT | P_READ_WRITE;
	}
	// create table for reserved_page //
	dir_idx = (RESV_PAGE/PAGE_SIZE)/1024;
	dir[dir_idx] = (uint32_t)page_calloc() | P_PRESENT | P_READ_WRITE;

}


static void dump_dir()
{
	int dir_idx, tbl_idx;
	dir_t *dir = (dir_t *)PDIR_ADDR;
	virt_t *table;
	for(dir_idx = 0; dir_idx < 1024; dir_idx++)
	{
		if(dir[dir_idx] & P_PRESENT) {
			kprintf("Dir: %d\n", dir_idx);
			for(tbl_idx=0; tbl_idx < 1024; tbl_idx++) {
				table = (uint32_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
				if(table[tbl_idx] & P_PRESENT) {
					kprintf("%d ",tbl_idx);
				}
			}
			kprintf("\n");
		}
	}
}

bool is_mapped(virt_t addr)
{
	dir_t *dir = (dir_t *) PDIR_ADDR;
	virt_t *table;
	if(addr & 0xFFF) {
		panic("Not aligned: %p\n", addr);
	}
	int dir_idx = (addr / PAGE_SIZE) / 1024;
	int tbl_idx = (addr / PAGE_SIZE) % 1024;
	if(!(dir[dir_idx] & P_PRESENT)) {
		return false;
	}
	table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
	if(!(table[tbl_idx] & P_PRESENT)) {
		return false;
	}
	return true;
}

phys_t virt_to_phys(virt_t addr)
{
	virt_t *table;
	int dir_idx = (addr / PAGE_SIZE) / 1024;
	int tbl_idx = (addr / PAGE_SIZE) % 1024;
	KASSERT(addr & 0xFFF);
	KASSERT(is_mapped(addr));
	table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
	return table[tbl_idx] & 0xFFFFF000;
}

static void invlpg(virt_t addr)
{
    asm volatile("invlpg (%0)"::"r" (addr) : "memory");
}

// todo more testing //
void unmap(virt_t virtual_addr)
{
	map(virtual_addr, 0, 0);
}

void map(virt_t virtual_addr, phys_t physical_addr, flags_t flags)
{
	dir_t *dir = (dir_t *) PDIR_ADDR;
	virt_t *table;
	int dir_idx = (virtual_addr / PAGE_SIZE) / 1024;
	int tbl_idx = (virtual_addr / PAGE_SIZE) % 1024;

	table = (virt_t *)(PTABLES_ADDR + dir_idx * PAGE_SIZE);

	if(! dir[dir_idx] & P_PRESENT) {
		dir[dir_idx] = (phys_t) page_alloc() | flags;
		invlpg((virt_t) table);
		memset(table, 0, PAGE_SIZE);
	}
	table[tbl_idx] = physical_addr | flags;
	invlpg(virtual_addr);
}

static void setup_heap()
{
	heap->start_addr = HEAP_START;
	heap->end_addr = HEAP_START + HEAP_INITIAL_SIZE;
	heap->max_addr = HEAP_END;
	heap->readonly = false;
	heap->supervisor = true;
	virt_t p;
	for(p = heap->start_addr; p <= heap->end_addr; p += PAGE_SIZE) {
		map(p, (phys_t) page_alloc(), P_PRESENT | P_READ_WRITE);
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
		frame = (phys_t) page_alloc();
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

void mem_init(multiboot_header *mb, phys_t kernel_end_addr)
{
	reserve_memory(mb, kernel_end_addr);
	kprintf("Available memory: %d pages, %d MB\n", num_pages, num_pages * 4/1024);
	kernel_dir = page_calloc();
	kernel_end_addr = ((kernel_end_addr / PAGE_SIZE)+1)*PAGE_SIZE;
	identity_map_kernel(kernel_dir, kernel_end_addr);
	recursively_map_page_directory(kernel_dir);

	switch_page_directory(kernel_dir);
	pg_on = true;

	setup_heap();
	//dump_dir();
}
