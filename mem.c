/*
	low level functions to allocate deallocate pages of memory
*/
#include "mem.h"
#include "kernel.h"
#include "multiboot.h"
#include "assert.h"
#include "isr.h"
#include <string.h>
#include <types.h>

#define PAGE_SIZE	0x1000 // 4Kb

extern void flush_tlb();
extern void switch_page_directory(unsigned int *dir);

// flagurile unei tabele //
// comune pt directory si page tables
#define P_PRESENT			1<<0	// pagina este in memoria fizica?
#define P_READ_WRITE		1<<1	// isset ? read/write : read
#define P_USER				1<<2	// isset ? acess all : access supervisor
#define P_WRITE_THROUGH		1<<3	// daca e setat, write-through caching enabled else write-back enabled
#define P_CACHE_DISABLED	1<<4	// valabil pt page directory - if set, page will not be cached
#define P_ACCESSED			1<<5	// a fost accesata
#define P_DIRTY				1<<6	// valabil pt page tables - if set, page was accessed
#define P_SIZE				1<<7	// valabil pt page directoru - pagini de 4K sau 4M
#define P_GLOBAL			1<<8	// if set, prevents TLB from updating the cache if CR3 is reset



/*
	Memoria arata cam asa:
	- code - 0x100000 (1MB)
		- functia start
	- data
	- bss (stiva)
	- kernel end == end bss (aliniat la 4096 bytes)
	- page_directory - ocupa 1024 * 4 bytes - 1 pagina
	- first_page_table - 1024 * 4 bytes
*/



char *ikalloc_ptr = 0;
char *ikalloc_mem_end = 0;
unsigned int num_frames;
unsigned int up_mem;
unsigned int *frames_stack;
unsigned int stack_idx = 0;
unsigned int *kernel_dir = NULL;

//
//	Very basic memory allocator (Initial KALLOC)- o sa imi aloce memorie, (ce nu va fi dealocata) 
//		pentru a initializa primele structuri de date. Alocarea incepe a o face la ikalloc_ptr si incrementeaza de fiecare data cu size.
//		presupune ca un char este pe 1 byte
//		dupa activare heap nu o sa o mai folosesc
//		aloca size bytes de memorie si intoarce un pointer catre zona
//
void *ikalloc(unsigned int size, int align) {
	void *tmp = 0;
	KASSERT(ikalloc_ptr != NULL);
	KASSERT(ikalloc_mem_end != NULL);

	// if align and not allready aligned
	if(align && ((unsigned int) ikalloc_ptr & 0xFFF)) { 
		ikalloc_ptr = (char *) ((unsigned int) ikalloc_ptr & 0xFFFFF000) + 0x1000;
	}
	KASSERT(ikalloc_ptr+size < ikalloc_mem_end);
	tmp = ikalloc_ptr;
	ikalloc_ptr += size;
	return tmp;
}

void push_frame(unsigned int frame_addr) {
	KASSERT((frame_addr & 0xFFFFF000) | (frame_addr == 0));
	frames_stack[stack_idx++] = frame_addr;
}
unsigned int pop_frame() {
	stack_idx--;
	KASSERT(stack_idx != num_frames);
	unsigned int frame_addr = frames_stack[stack_idx];
	frames_stack[stack_idx] = 0;
	return frame_addr;
}
void remove_frame(unsigned int frame_addr){
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
}

//
//
//
void map_linear_to_any(unsigned int *dir, unsigned int linear_addr, unsigned int flags) {
	unsigned short int dir_idx = (linear_addr & 0xFFC00000) >> 22;
	unsigned short int table_idx = (linear_addr & 0x3FF000) >> 12;
	unsigned int t, p;
	unsigned int *table;

	KASSERT((linear_addr & 0xFFFFF000) | (linear_addr == 0));

	if(dir[dir_idx] == 0) { // if table is not present;
		t = (unsigned) ikalloc(1024 * sizeof(int), 1);  // give me a page
		dir[dir_idx] = t|P_PRESENT|P_READ_WRITE;
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000);
	KASSERT(table[table_idx] == 0);
		
	p = pop_frame();
	
	table[table_idx] = p|flags;
}



//
//	Like the above, but will map to a known physical address - used in kernel mapping
//
void map_linear_to_physical(unsigned int *dir, unsigned int linear_addr, unsigned int physical_addr, unsigned int flags) {
	unsigned short int dir_idx = (linear_addr & 0xFFC00000) >> 22;
	unsigned short int table_idx = (linear_addr & 0x3FF000) >> 12;
	unsigned int t, p;
	unsigned int *table;

	KASSERT((linear_addr & 0xFFFFF000) | (linear_addr == 0));

	if(dir[dir_idx] == 0) { // if table is not present;
		t = (unsigned) ikalloc(1024 * sizeof(int), 1);  // give me a page
		dir[dir_idx] = t|P_PRESENT|P_READ_WRITE;
	}
	table = (unsigned int *) (dir[dir_idx] & 0xFFFFF000);
	KASSERT(table[table_idx] == 0);
		
	p = physical_addr;
	remove_frame(physical_addr);
	
	table[table_idx] = p|flags;
}

void map_range_any(unsigned int *dir, unsigned int linear_start, unsigned int linear_end, unsigned int flags) {
	unsigned int i;
	KASSERT((linear_start & 0xFFFFF000) | (linear_start == 0));
	KASSERT((linear_end & 0xFFFFF000));
	for(i=linear_start; i < linear_end; i+=PAGE_SIZE) {
		map_linear_to_any(dir, i, flags);
	}
}

void map_range_physical(unsigned int *dir, unsigned int linear_start, unsigned int linear_end, unsigned int physical_start, unsigned int flags) {
	unsigned int i;
	KASSERT((linear_start & 0xFFFFF000) | (linear_start == 0));
	KASSERT((linear_end & 0xFFFFF000));
	KASSERT((linear_end - linear_start)+physical_start < num_frames*PAGE_SIZE);
	KASSERT(linear_start < linear_end);
	unsigned int pages = (linear_end - linear_start) / PAGE_SIZE;
	for(i=0; i < pages; i++) {
		map_linear_to_physical(dir, linear_start+i*PAGE_SIZE, physical_start+i*PAGE_SIZE, flags);
	}
}

unsigned int allocated_frames() {
	return num_frames - stack_idx;
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


void mem_init(multiboot_header *mboot) 
{
	unsigned int i;
	
	// aproximate memory, to keep things simple //
	up_mem = (((unsigned int) mboot->mem_upper/1024 ) + 1) * 1024 * 1024;
	num_frames = (0x100000 + up_mem) / PAGE_SIZE;
	
	// setup ikalloc_ptr //
	ikalloc_ptr = (char *) kinfo.end;
	ikalloc_mem_end = (char *) (num_frames * PAGE_SIZE);	// setat initial pt toata memoria, mai tarziu o sa fac shrink si o sa invalidez ikalloc, ca sa nu faca overflow


	// setup frames_stack //
	frames_stack = (unsigned int *) ikalloc(num_frames * sizeof(int), 1);
	memset(frames_stack, '\0', num_frames * sizeof(int));

	for(i=0; i < num_frames; i++) {
		push_frame(i*PAGE_SIZE);
	}

	// setup kernel dir //
	kernel_dir = ikalloc(PAGE_SIZE, 1);		// 1024 integers
	memset(kernel_dir, '\0', PAGE_SIZE);
	
	map_range_physical(kernel_dir, 0xB8000, (unsigned int) ikalloc_ptr, 0xB8000, P_PRESENT | P_READ_WRITE);
	
	map_linear_to_any(kernel_dir, 0xDEADC000, P_PRESENT | P_READ_WRITE);
	
	switch_page_directory(kernel_dir);

	unsigned int *x = (unsigned int *) 0xDEADC0DE;
	x[0] = 0xFACECACA;
	kprintf("Writed at linear address 0x%X (%ud), value: 0x%X\n", x, x, x[0]);
	
	kprintf("%d total frames, %d allocated frames\n", num_frames, allocated_frames());
	kprintf("%dKb total memory, %dKb used, %dKb free\n", num_frames*PAGE_SIZE/1024, allocated_frames()*PAGE_SIZE/1024, (num_frames-allocated_frames())*PAGE_SIZE/1024);
		
}
