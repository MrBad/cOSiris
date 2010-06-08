#include <types.h>
#include <string.h>
#include "kheap.h"
#include "console.h"
#include "mem.h"
#include "assert.h"


static mem_block_t *start_block = NULL;		// points to the base of the list
unsigned int heap_ok = 0;					// was heap inited
unsigned int kheap_top = 0;					// end addr of the list

unsigned int kheap_expand(unsigned int inc_num_bytes) {
	unsigned int i, p;
	unsigned int pages = (inc_num_bytes / PAGE_SIZE) + 1;
	pages = pages < KHEAP_MIN_EXPAND_PAGES ? KHEAP_MIN_EXPAND_PAGES : pages;
	
	KASSERT((kheap_top + pages * PAGE_SIZE) < KHEAP_END);

	//kprintf("-expand heap with %d, to 0x%x\n", pages, kheap_top);
	
	for(i = 0; i < pages; i++) {
		p = pop_frame();
		map_linear_to_physical(kheap_top+i*PAGE_SIZE, p, P_PRESENT|P_READ_WRITE);
	}
	flush_tlb();
	kheap_top += pages * PAGE_SIZE;
	return pages * PAGE_SIZE;
}

void kheap_contract() {
	panic("kheap_contract() unimplemented");
}

void kheap_dump() {
	mem_block_t *p;
	p = start_block;
	while(p) {
		kprintf("ptr: 0x%X, prev: 0x%X, next: 0x%X, sz: %uKb, free: %u, \n", p, p->prev, p->next, p->size/1024, p->is_free);
		p = p->next;
	}
}

void *kalloc(unsigned int nbytes) {
	
	mem_block_t *p, *k;
	unsigned short int found;
	unsigned int numbytes = 0;
	// if start block is empty, init the list //
	// in the first stage, list will contain one member - start_block - which will map the KHEAP_INIT_SIZE memory //
	if(! start_block) {
		start_block = (mem_block_t *) KHEAP_START;
		start_block->magic = KHEAP_MAGIC;
		start_block->size = KHEAP_INIT_SIZE - sizeof(mem_block_t); // init size minus size of start_block //
		start_block->is_free = 1;
		start_block->next = NULL;
		start_block->prev = NULL;
		
		kheap_top = KHEAP_INIT_SIZE + KHEAP_START;
		heap_ok = 1;
	}
	
	p = start_block;
	found = 0;

	for(p = start_block, found = 0; p; p = p->next) {

		if(p->is_free) {
			// if this is the last node and we don't have enough space //
			if(p->next == NULL && p->size <= nbytes+sizeof(mem_block_t)) { // less and equal will always keep few free bytes at the top of the heap
				numbytes = kheap_expand(nbytes);
				if(! numbytes) {
					panic("Cannot expand kheap\n");
				}
				p->size += numbytes;
			}

			if(p->size == nbytes) {
				p->is_free = 0;
				found = 1;
			} else { // found a big enough block, which will be split - p will point to new size, k to the rest of free space
				k = p;
				
				k = (mem_block_t *) ((unsigned int)k + sizeof(mem_block_t) + nbytes); // jump nbytes+sizeof struct
				k->magic = KHEAP_MAGIC;
				k->size = p->size - nbytes - sizeof(mem_block_t); // new shrinked free space
				k->is_free = 1;
				k->next = p->next;
				k->prev = p;
				
				p->size = nbytes;
				p->is_free = 0;
				p->next = k;
				found = 1;
			}

			if(found) {
				break;
			}
		}
	}
	KASSERT(found);
	return found ? p+1 : NULL;
}

void kfree(void *ptr) {
	mem_block_t *p;
	p = ptr - sizeof(mem_block_t);
	if(p->magic != KHEAP_MAGIC) {
		// find prev magic //
		panic("Invalid magic number in kfree: 0x%x\n", p);
		return;
	}
	p->is_free = 1;
	if(p->prev) {
		if(p->prev->is_free) {
			p->prev->size += p->size + sizeof(mem_block_t);
			p->prev->next = p->next;
			p->next->prev = p->prev;
			p->magic = 0;
			p = p->prev;
		}
	}
	if(p->next) {
		if(p->next->is_free) {
//			kprintf("Next is free: 0x%X\n", p->next);
			p->size += p->next->size + sizeof(mem_block_t);
			p->next->magic=0;
			p->next = p->next->next;
		}
	}
}