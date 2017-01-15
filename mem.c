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

int fault_counter = 0;
/**
 * Page fault isr
 */
int page_fault(struct iregs *r)
{
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
		heap_dump();
		debug_dump_list(first_block);
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
phys_t *frame_alloc()
{
	phys_t *addr;
	KASSERT(last_free_page != 0);
	addr = (phys_t *)last_free_page;
	if(paging_on()) {
		map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
		last_free_page = *((phys_t *)RESV_PAGE);
		*((phys_t *)RESV_PAGE) = 0;
		unmap(RESV_PAGE);
	} else {
		last_free_page = *addr;
		*addr = 0; // unlink next //
	}
	num_pages--;
	return addr;
}

// allocs and bzero PAGE_SIZE frame //
phys_t *frame_calloc()
{
	phys_t *addr;
	addr = frame_alloc();
	if(paging_on()) {
		map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
		memset((void *)RESV_PAGE, 0, PAGE_SIZE);
		unmap(RESV_PAGE);
	} else {
		memset(addr, 0, PAGE_SIZE);
	}
	return addr;
}

void frame_free(phys_t addr)
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

static phys_t round_page_up(phys_t addr) {
	return ((addr/PAGE_SIZE)+1) * PAGE_SIZE;
}

static void reserve_region(phys_t addr, size_t length, multiboot_header *mb)
{
	struct	kinfo_t kinfo;
	phys_t	ptr;
	phys_t	initrd_location = 0,
			initrd_end = 0;

	if (mb && mb->mods_count > 0) {
		initrd_location = *((unsigned int *) mb->mods_addr);
		initrd_end = *(unsigned int *) (mb->mods_addr + 4);
		initrd_end = round_page_up(initrd_end);
	}

	get_kernel_info(&kinfo);

	for(ptr = addr; ptr - addr < length; ptr+=PAGE_SIZE) {
		// we will not use first frame //
		if(ptr == 0) {
			*((phys_t *) addr) = 0;
			continue;
		}
		// skip frames from vga start addr to end of the kernel //
		if(ptr >= VGA_FB_ADDR && ptr <= kinfo.end) {
			continue;
		}
		// skip frames that belongs to initrd section //
		if(initrd_location) {
			if(ptr >= initrd_location && ptr <= initrd_end) {
				continue;
			}
		}

		frame_free(ptr);
	}
}



static void reserve_memory(multiboot_header *mb)
{
	memory_map *mm;
	mm = (memory_map *) mb->mmap_addr;
	do {
		if(mm->type == 1) {
			phys_t addr;
			size_t length;
			addr = (mm->base_addr_high << 16) + mm->base_addr_low;
			length = (mm->length_high << 16) + mm->length_low;
			reserve_region(addr, length, mb);
		}
		mm = (memory_map *) ((unsigned long) mm + mm->size + sizeof(mm->size));
	} while((unsigned long) mm < mb->mmap_addr + mb->mmap_length);
}

static void recursively_map_page_directory(dir_t *dir)
{
	dir[1023] = (phys_t) dir | P_PRESENT | P_READ_WRITE;
}

// identity maps a PAGE_SIZE page located at addr
static void identity_map_page(dir_t *dir, phys_t addr)
{
	phys_t *table;
	int dir_idx, tbl_idx;
	dir_idx = (addr / PAGE_SIZE) / 1024;
	tbl_idx = (addr / PAGE_SIZE) % 1024;
	if(!(dir[dir_idx] & P_PRESENT)) {
		dir[dir_idx] = (uint32_t) frame_calloc() | P_PRESENT | P_READ_WRITE;
	}
	table = (phys_t *)(dir[dir_idx] & 0xFFFFF000);
	table[tbl_idx] = addr | P_PRESENT | P_READ_WRITE;
}

static void identity_map_kernel(dir_t *dir, multiboot_header *mb)
{
	struct kinfo_t kinfo;
	phys_t addr;

	get_kernel_info(&kinfo);

	// identity maps pages from VGA addr to kernel end //
	for(addr = VGA_FB_ADDR; addr <= kinfo.end; addr += PAGE_SIZE) {
		identity_map_page(dir, addr);
	}

	// identity maps from initrd_location to initrd_end -> used by initrd, later
	// maibe we will move it up into the memory and not ident,
	if (mb->mods_count > 0) {
		phys_t	initrd_location = 0, initrd_end = 0;
		initrd_location = *((unsigned int *) mb->mods_addr);
		initrd_end = *(unsigned int *) (mb->mods_addr + 4);
		initrd_end = round_page_up(initrd_end);
		for(addr = initrd_location; addr <= initrd_end; addr += PAGE_SIZE) {
			identity_map_page(dir, addr);
		}
	}

	// create table for reserved_page //
	int dir_idx;
	dir_idx = (RESV_PAGE/PAGE_SIZE) / 1024;
	dir[dir_idx] = (uint32_t)frame_calloc() | P_PRESENT | P_READ_WRITE;
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
		kprintf("is_mapped(): Not aligned: %p\n", addr);
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
		dir[dir_idx] = (phys_t) frame_alloc() | flags;
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
	for(p = heap->start_addr; p < heap->end_addr; p += PAGE_SIZE) {
		map(p, (phys_t) frame_alloc(), P_PRESENT | P_READ_WRITE);
	}
}

void *sbrk(unsigned int increment) {
	unsigned int start = heap->end_addr;
	unsigned int frame, i;
	unsigned int iterations = increment / PAGE_SIZE;
	KASSERT(heap);
	KASSERT(!(increment & 0xFFF));
	if (iterations < 1) {
		iterations++;
	}
	for (i = 0; i < iterations; ++i) {
		frame = (phys_t) frame_alloc();
		KASSERT(!(heap->end_addr & 0xFFF));
		KASSERT(frame);
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

void mem_init(multiboot_header *mb)
{
	reserve_memory(mb);
	kprintf("Available memory: %d pages, %d MB\n", num_pages, num_pages * 4/1024);
	kernel_dir = frame_calloc();
	identity_map_kernel(kernel_dir, mb);
	recursively_map_page_directory(kernel_dir);

	switch_page_directory(kernel_dir);
	pg_on = true;

	setup_heap();
	//dump_dir();
}
