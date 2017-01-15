#include "console.h"
#include "mem.h"
#include "kheap.h"
#include "assert.h"
#include "include/types.h"
#include "include/string.h"

#define MAGIC_HEAD 0xDEADC0DE
#define MAGIC_END 0xDEADBABA

extern void halt();

extern bool heap_active;

heap_t myheap = {0};
heap_t *heap = &myheap;
// block_meta_t *first_block = 0;

//unsigned int calls = 0;
//extern page_directory_t *kernel_dir;

void debug_dump_list(block_meta_t *p) {
//	block_meta_t *p;
//	p = first_block;
	while (p) {
		KASSERT(p->magic_head == MAGIC_HEAD);
		KASSERT(p->magic_end == MAGIC_END);
		kprintf("node: 0x%X, magic_head: 0x%X, magic_end: %X, size: %i, %s, next: 0x%X\n", p+1, p->magic_head,
		        p->magic_end,
		        p->size, p->free ? "free" : "used", p->next);
		p = p->next;
	}
}

void heap_dump()
{
	kprintf("HEAP\tstart_addr: %p, end_addr: %p, max_addr: %p, \n", heap->start_addr, heap->end_addr, heap->max_addr);
}

void init_first_block()
{
	KASSERT(HEAP_INITIAL_SIZE > sizeof(block_meta_t));
	kprintf("Setup initial heap, at: 0x%X, initial size: 0x%X\n", HEAP_START, HEAP_INITIAL_SIZE);
	first_block = (block_meta_t *) HEAP_START;
	first_block->free = true;
	first_block->size = HEAP_INITIAL_SIZE - sizeof(block_meta_t);
	first_block->magic_head = MAGIC_HEAD;
	first_block->magic_end = MAGIC_END;
	first_block->next = NULL;
}

void *malloc(unsigned int nbytes) {
	block_meta_t *p, *n;
	unsigned int next_size;
	char *c;
	nbytes = (nbytes / 4 + 1) * 4; // align to 4 bytes

	if (first_block == NULL) {
		KASSERT(HEAP_INITIAL_SIZE > sizeof(block_meta_t));
		kprintf("Setup initial heap, at: 0x%X, initial size: 0x%X\n", HEAP_START, HEAP_INITIAL_SIZE);
		first_block = (block_meta_t *) HEAP_START;
		first_block->free = true;
		first_block->size = HEAP_INITIAL_SIZE - sizeof(block_meta_t);
		first_block->magic_head = MAGIC_HEAD;
		first_block->magic_end = MAGIC_END;
		first_block->next = NULL;
	}
	KASSERT(first_block != NULL);
	KASSERT(first_block->magic_head == MAGIC_HEAD);
	KASSERT(first_block->magic_end == MAGIC_END);
	p = first_block;

	while (p) {
			// kprintf(".");
		if (!p->free) {
			p = p->next;
			continue;
		}
		if ((p->size < nbytes + sizeof(block_meta_t)) && p->next == NULL) {
			// kprintf(".");
			sbrk(PAGE_SIZE * 2);
			p->size += PAGE_SIZE * 2;
			continue;
		}
		else {
//			kprintf("SPLIT; p: 0x%X orig_size: %i, nbytes: %i, meta: %i\n",p+1, p->size, nbytes, sizeof(block_meta_t));
			c = (char *) p;
			next_size = p->size - nbytes - sizeof(block_meta_t);
			n = p->next;
			p->size = nbytes;
			p->free = false;
			p->next = (block_meta_t *) (c + sizeof(block_meta_t) + nbytes);
			if((unsigned int)p->next <= heap->end_addr) { // happens on last and large blocks
				sbrk(PAGE_SIZE * 2);
				KASSERT(p->next->next == NULL);
				p->next->size += PAGE_SIZE * 2;
			}
			p->next->free = true;
			p->next->size = next_size;
			p->next->magic_head = MAGIC_HEAD;
			p->next->magic_end = MAGIC_END;
			p->next->next = n;
			return p + 1;
		}
		// p = p->next;
	}
//	debug_dump_list();

	kprintf("%X\n", p);
	kprintf("Out of memory: malloc %u\n", nbytes);
	halt();
	return NULL;
}

void free(void *ptr) {
	block_meta_t *p, *prev;
	p = first_block;
	for (; ;) {
		if (p + 1 == ptr) {
			KASSERT(p->magic_head == MAGIC_HEAD);
			KASSERT(p->magic_end == MAGIC_END);
			p->free = true;
			if (p->next && p->next->free) {
				p->size += p->next->size + sizeof(block_meta_t);
				p->next = p->next->next;
			}
			if (prev && prev->free) {
				prev->size += p->size + sizeof(block_meta_t);
				prev->next = p->next;
			}
			break;
		}
		if (!p->next) {
			kprintf("Bad free pointer: %p, mem: %p, size: %d, mgh: %X, mge: %X, %s\n", ptr, p+1, p->size, p->magic_head, p->magic_end, p->free ? "free":"used");
			halt();
			break;
		}
		prev = p;
		p = p->next;
	}
}

void *calloc(unsigned nbytes) {
	void *p = malloc(nbytes);
	memset(p, 0, nbytes);
	return p;
}

void *malloc_page() {
	char *mem = malloc(PAGE_SIZE * 3);
	unsigned int mem_start, mem_end, mem_length, new_start;
	mem_start = (unsigned int) mem;
	mem_length = 3 * PAGE_SIZE;
	mem_end = mem_start + mem_length;
	block_meta_t *p, *n;
	if(mem_start & 0xFFF){ // if is not PAGE_SIZE alligned
		new_start = (mem_start & 0xFFFFF000) + PAGE_SIZE;
		if(new_start - mem_start < sizeof(block_meta_t)) {
			new_start += PAGE_SIZE;
		}
		p = ((block_meta_t *) mem_start) - 1;
		n = ((block_meta_t *) new_start) - 1;
		n->free = false;
		n->next = p->next;
		n->magic_head = MAGIC_HEAD;
		n->magic_end = MAGIC_END;
		n->size = mem_end - new_start;
		p->size = new_start - mem_start - sizeof(block_meta_t);
		p->next = n;
		KASSERT(n->size >= PAGE_SIZE);
		KASSERT((unsigned int)n->next > (new_start + n->size));
		free(p+1);
		return n + 1;
	}
	return mem;
}

void heap_init() {

	//kprintf("In heap \n");
	KASSERT((HEAP_INITIAL_SIZE / PAGE_SIZE) > 0);
	if (!heap) {
		panic("Heap is not initialized\n");
	}
	init_first_block();
	unsigned int i;
	char *p;

	kprintf("heap_init(): heap->end_addr: %X, size: x%X\n", heap->end_addr, heap->end_addr-heap->start_addr);
	free(malloc(1));
	char *k[3000];
	kprintf("Alloc\n");
	for (i = 2000; i < 3000; i++) {
		p = malloc(i);
		memset(p, 0, i);

		k[i] = p;
	}

	kprintf("Freeing\n");
	for (i = 2000; i < 3000; i++) {
		free(k[i]);
	}
	kprintf("OK\n");


// char *p = malloc(4080);
// kprintf("%p\n", p);
//
// char *page;
// page = malloc_page();
// kprintf("%p\n", page);
// memset(page, 0, PAGE_SIZE);
// memset(p, 0, 4080);
// free(page);
// free(p);


	// unsigned int i; char *p;
	// char *k = (char *) malloc(4096*10*sizeof(int));
	// for (i = 4096; i < 5000; i++) {
	// 	p = malloc(i);
	// 	memset(p, 0, i);
	// 	k[i] = p;
	// }
	// for (i = 4096; i < 5000; i++) {
	// 	free((void *)k[i]);
	// }
	// free(k);


//	debug_dump_list(first_block);
	// kprintf("heap->end_addr: %X, size: %iMB\n", heap->end_addr, (heap->end_addr-heap->start_addr)/1024/1024);
//	halt();

//	p=malloc(10);
//	memset(p, 'A', 10);
	debug_dump_list(first_block);
}
