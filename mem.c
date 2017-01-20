#include <types.h>		// types //
#include "string.h"		// memset //
#include "console.h"	// kprint, panic//
#include "assert.h"		// KASSERT /
#include "isr.h" 		// iregs //
#include "multiboot.h"	// multiboot_header //
#include "kernel.h" 	// kinfo_t //
#include "kheap.h"		// HEAP defs //
#include "mem.h"
#include "x86.h"

// extern void halt(void);

phys_t	last_free_page = 0;
size_t	num_pages = 0;
dir_t	*kernel_dir = NULL;
bool pg_on = false;

extern void shutdown();

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
	shutdown();
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
	if(paging_on()){
		map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
		*((phys_t *)RESV_PAGE) = last_free_page;
		unmap(RESV_PAGE);
	} else {
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
	if(paging_on()){
		map(RESV_PAGE, (phys_t) dir, P_PRESENT | P_READ_WRITE);
		((virt_t *) RESV_PAGE)[1023] = (phys_t)dir | P_PRESENT | P_READ_WRITE;
		unmap(RESV_PAGE);
	} else {
		dir[1023] = (phys_t) dir | P_PRESENT | P_READ_WRITE;
	}
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
	phys_t addr;

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


void dump_dir()
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
	KASSERT(!(addr & 0xFFF));
	if(! is_mapped(addr)) {
		panic("%p is not mapped\n", addr);
	}
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
		map(p, (phys_t) frame_alloc(), P_PRESENT | P_READ_WRITE | P_USER);
	}
}

void *sbrk(unsigned int increment) {
	unsigned int start = heap->end_addr;
	unsigned int frame, i;
	unsigned int iterations;
	KASSERT(heap);
	KASSERT(!(increment & 0xFFF));
	if(increment == 0) {
		return (void *)heap->end_addr;
	}
	iterations = increment / PAGE_SIZE;
	if (iterations < 1) {
		iterations++;
	}
	for (i = 0; i < iterations; ++i) {
		frame = (phys_t) frame_alloc();
		KASSERT(!(heap->end_addr & 0xFFF));
		KASSERT(frame);
		map(heap->end_addr, frame, P_PRESENT);
		heap->end_addr += PAGE_SIZE;
		if (heap->end_addr >= heap->max_addr) {
			panic("Out of memory in sbrk: heap end addr: 0x%X08, size: %iM\n", heap->end_addr,
					(heap->end_addr - heap->start_addr) / 1024 / 1024);
		}
	}
	return (void *) start;
}


// to be tested more //
// so ugly code, but i preffered indirect temp mapping via RESV_PAGE instead using malloc_page_aligned
// witch is not yet safe nor mature //
// also - using direct frame assigning is more efficient
// also i did not turn off paging...
// dir_t *clone_directory()
// {
// 	cli();
// 	dir_t *new_dir = frame_calloc();
// 	// memset(new_dir, 0, PAGE_SIZE);
// 	dir_t *curr_dir = (dir_t *)PDIR_ADDR;
// 	virt_t *table;
// 	phys_t *new_table;
// 	int dir_idx=0, tbl_idx=0;
// 	unsigned char *p, *k;
//
// 	for(dir_idx = 0; dir_idx < 1023; dir_idx++) {
// 		if(curr_dir[dir_idx] & P_PRESENT) {
//
// 			if(dir_idx == 0) { // link kernel entries //
// 				map(RESV_PAGE, (phys_t) new_dir, P_PRESENT | P_READ_WRITE);
// 				((dir_t *)RESV_PAGE)[dir_idx] = curr_dir[dir_idx];
// 				unmap(RESV_PAGE);
// 				// new_dir[dir_idx] = curr_dir[dir_idx];
// 			}
// 			else {
//
// 				kprintf("{DIR:%d, %p\n", dir_idx, table);
// 				new_table = frame_calloc();
//
// 				map(RESV_PAGE, (phys_t) new_dir, P_PRESENT | P_READ_WRITE);
// 				((dir_t *)RESV_PAGE)[dir_idx] = (phys_t)new_table | P_PRESENT | P_READ_WRITE;
// 				unmap(RESV_PAGE);
//
// 				// new_dir[dir_idx] = (phys_t)new_table | P_PRESENT | P_READ_WRITE | P_USER;
// 				if(dir_idx == 1022) {
// 					kprintf("---->----\n");
// 					continue;
// 				}
// 				table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
// 				for(tbl_idx = 0; tbl_idx < 1024; tbl_idx++) {
// 					if(table[tbl_idx] & P_PRESENT) {
// 						p = (char *)(dir_idx*1024*PAGE_SIZE+tbl_idx*PAGE_SIZE);
// 						k = (char *)RESV_PAGE;
// 						kprintf("-[%p-\n", p);
// 						void *page = frame_calloc();
// 						map(RESV_PAGE, (phys_t) page, P_PRESENT | P_READ_WRITE);
// 						// memcpy((void *)RESV_PAGE, (void *) (dir_idx*1024*PAGE_SIZE+tbl_idx*PAGE_SIZE), PAGE_SIZE);
//
// 						for(;(unsigned int)k < RESV_PAGE+PAGE_SIZE;p++,k++) {
// 							*p = *k;
// 						}
// 						unmap(RESV_PAGE);
//
// 						map(RESV_PAGE, (phys_t) new_table, P_PRESENT | P_READ_WRITE);
// 						((virt_t *) RESV_PAGE)[tbl_idx] = (phys_t) page | P_PRESENT | P_READ_WRITE;
// 						unmap(RESV_PAGE);
// 					}
// 				}
// 			}
// 		}
// 	}
// 	kprintf("OK\n");
// 	sti();
// 	return new_dir;
// }

virt_t *temp_map(phys_t *page)
{
	map(RESV_PAGE, (phys_t)page, P_PRESENT|P_READ_WRITE);
	return (virt_t *)RESV_PAGE;
}

void temp_unmap()
{
	unmap(RESV_PAGE);
}


dir_t *clone_directory()
{
	static int iter = 0;
	int dir_idx, tbl_idx;
	dir_t *curr_dir = (dir_t *)PDIR_ADDR;
	dir_t *new_dir = (dir_t *)frame_calloc();
	phys_t *new_table;
	virt_t *table, *addr;
	phys_t *frame;
	virt_t *from_addr;
	extern unsigned int stack_size;

	cli();
	for(dir_idx = 0; dir_idx < 1023; dir_idx++) {
		if(curr_dir[dir_idx] & P_PRESENT) {
			// from 0 to kernel stack, we link the pages
			if(dir_idx < ((KERNEL_STACK_HI-stack_size)/1024/PAGE_SIZE)) {
				addr = temp_map(new_dir);
				addr[dir_idx] = curr_dir[dir_idx];
				temp_unmap();
				// kprintf("Link dir: %p\n", (dir_idx * 1024 * PAGE_SIZE));
			}
			// we clone the rest
			else {
				new_table = frame_calloc();
				addr = temp_map(new_dir);
				addr[dir_idx] = (phys_t)new_table | P_PRESENT | P_READ_WRITE;
				temp_unmap();

				table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
				for(tbl_idx = 0; tbl_idx < 1024; tbl_idx++) {
					if(table[tbl_idx] & P_PRESENT) {
						frame = (phys_t *)frame_calloc();
						from_addr = (virt_t *)(dir_idx * 1024 * PAGE_SIZE + tbl_idx * PAGE_SIZE);
						//kprintf("copy phys: %p->%p\n", from_addr, frame);
						addr = temp_map(frame);
						memcpy(addr, from_addr, PAGE_SIZE);
						temp_unmap();

						addr = temp_map(new_table);
						addr[tbl_idx] = (phys_t)frame | P_PRESENT | P_READ_WRITE;
						temp_unmap();
					}
				}
			}
		}
	}

	addr = temp_map(new_dir);
	addr[1023] = (phys_t)new_dir | P_PRESENT | P_READ_WRITE;
	temp_unmap();
	iter++;
	sti();
	return new_dir;
}


// Moving stack up in memory (KERNEL_STACK_HI)
// so clone_directory() will clone it instead of linking it
void move_stack_up()
{
	KASSERT(pg_on);
	cli();
	phys_t *frame;
	extern unsigned int stack_ptr, stack_size;
	unsigned int *p, *k, i;
	unsigned int offset = KERNEL_STACK_HI - stack_ptr;

	for(i = stack_size / PAGE_SIZE; i; i--) {
		frame = frame_calloc();
		map(KERNEL_STACK_HI - PAGE_SIZE * i, (unsigned int)frame, P_PRESENT | P_READ_WRITE);
		for(p = (unsigned int *)(stack_ptr-PAGE_SIZE*i); (unsigned int)p < stack_ptr-PAGE_SIZE*(i-1); p++) {
			k = (unsigned int*)((unsigned int)p+offset);
			if(*p < stack_ptr && *p >= stack_ptr-stack_size) {
				*k = (unsigned int)*p + offset;
			} else {
				*k = *p;
			}
		}
	}

	unsigned int old_esp, old_ebp, new_esp, new_ebp;
	asm volatile("movl %%esp, %0" : "=r"(old_esp));
	asm volatile("movl %%ebp, %0" : "=r"(old_ebp));
	new_esp = old_esp + offset;
	new_ebp = old_ebp + offset;
	asm volatile("movl %0, %%esp"::"r"(new_esp));
	asm volatile("movl %0, %%ebp"::"r"(new_ebp));
	sti();
}

// inline me, so we don't modify esp, ebp //
inline void switch_page_directory (dir_t *dir) __attribute__((always_inline));
inline void switch_page_directory(dir_t *dir)
{
	asm volatile("mov %0, %%cr3":: "r"(dir));
	unsigned int cr0;
	asm volatile("mov %%cr0, %0": "=r"(cr0));
	cr0 |= 0x80000000;
	asm volatile("mov %0, %%cr0":: "r"(cr0));
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
	move_stack_up();

	setup_heap();
	heap_init(mb);

	// dump_dir();
	// // kprintf("Curr dir at: %p, kdir: %p\n", virt_to_phys(PDIR_ADDR), kernel_dir);

	// dir_t *new_dir = clone_directory();
	// kprintf("switching 2\n");
	// switch_page_directory(new_dir);

	// dump_dir();
	kprintf("Curr dir at: %p, kdir: %p\n", virt_to_phys(PDIR_ADDR), kernel_dir);
	// heap_dump();
}
