#include "x86.h"
#include "console.h"
#include "serial.h"
#include "mem.h"
#include "kheap.h"
#include "assert.h"
#include "include/types.h"
#include "include/string.h"

#define MAGIC_HEAD 0xDEADC0DE
#define MAGIC_END 0xDEADBABA

spin_lock_t kheap_lock;
extern bool heap_active;

heap_t myheap = {0};
heap_t *heap = &myheap;
extern int getpid();

void debug_dump_list(block_meta_t *p)
{

	static int dplitr = 0;
	while (p) {
		KASSERT(p->magic_head == MAGIC_HEAD);
		KASSERT(p->magic_end == MAGIC_END);
		if(dplitr++ < 10) {
			kprintf("pid:%d, node: 0x%X, ptr: %p, size: %8d, %s, next: 0x%X\n", getpid(), p, p+1,
		        p->size, p->free ? "free" : "used", p->next);
		} else {
			serial_debug("pid:%d, node: 0x%X, ptr: %p, size: %8d, %s, next: 0x%X\n", getpid(), p, p+1,
						p->size, p->free ? "free" : "used", p->next);
		}
		KASSERT(p != p->next);
		p = p->next;
		// if(dplitr++ > 10) {panic("Too many iterations\n");}
	}
	halt();
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

void *malloc(unsigned int nbytes)
{
	spin_lock(&kheap_lock);
	block_meta_t *p, *n;
	unsigned int next_size;
	char *c;
	if(nbytes % 4 != 0)
		nbytes = (nbytes / 4 + 1) * 4; // align to 4 bytes

	KASSERT(first_block != NULL);
	KASSERT(first_block->magic_head == MAGIC_HEAD);
	KASSERT(first_block->magic_end == MAGIC_END);
	p = first_block;
	unsigned int search_size = nbytes + sizeof(block_meta_t);
	while (p) {
			// kprintf(".");
		if (!p->free) {
			p = p->next;
			continue;
		}
		// if last node, and we don't have required size,
		if (p->next == NULL && p->size < search_size) {
			// kprintf("+");
			sbrk(PAGE_SIZE);
			p->size += PAGE_SIZE;
			continue;
		}
		// if not last node and we have the size
 		else if(p->size >= search_size) {
			// kprintf("SPLIT; p: 0x%X orig_size: %i, nbytes: %i, meta: %i\n",p+1, p->size, nbytes, sizeof(block_meta_t));
			KASSERT(p->size >= nbytes + sizeof(block_meta_t));
			KASSERT((unsigned int)(p+1) < heap->end_addr);
			c = (char *) p;
			next_size = p->size - nbytes - sizeof(block_meta_t);
			n = p->next;
			p->size = nbytes;
			p->free = false;
			p->next = (block_meta_t *) (c + sizeof(block_meta_t) + nbytes);
			p->next->free = true;
			p->next->size = next_size;
			p->next->magic_head = MAGIC_HEAD;
			p->next->magic_end = MAGIC_END;
			p->next->next = n;
			spin_unlock(&kheap_lock);
			return p + 1;
		}
		p = p->next;
	}
//	debug_dump_list();

	kprintf("%X\n", p);
	kprintf("Out of memory: malloc %u\n", nbytes);
	halt();
	return NULL;
}

static void heap_contract()
{
	spin_lock(&kheap_lock);
	block_meta_t *p = first_block;
	virt_t addr;
	virt_t new_end;

	while (p->next) {
		p = p->next;
	}
	if(p->size > HEAP_INITIAL_SIZE) {
		KASSERT((unsigned int)(p+1) + p->size == heap->end_addr);
		new_end = ((unsigned int)(p+1) & 0xFFFFF000) + HEAP_INITIAL_SIZE;
		KASSERT(is_mapped(new_end));
		p->size = new_end - (unsigned int)(p+1);
		for(addr = new_end; addr < heap->end_addr; addr+=PAGE_SIZE) {
			KASSERT(is_mapped(addr));
			frame_free(virt_to_phys(addr));
			unmap(addr);
		}
		heap->end_addr = new_end;
	}
	spin_unlock(&kheap_lock);
}

void free(void *ptr)
{
	block_meta_t *p, *prev = NULL;
	spin_lock(&kheap_lock);
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
			kprintf("Bad free pointer: %p, mem: %p, size: %d, mgh: %X, mge: %X, %s\n",
				ptr, p+1, p->size, p->magic_head, p->magic_end, p->free ? "free":"used");
			halt();
			break;
		}
		prev = p;
		p = p->next;
	}
	spin_unlock(&kheap_lock);
	heap_contract();
}


void *calloc(unsigned int nmemb, unsigned int size)
{
	unsigned int nbytes = nmemb * size;
	void *p = malloc(nbytes);
	memset(p, 0, nbytes);
	return p;
}

void *realloc(void *ptr, size_t size)
{
	void *n;
	if(!ptr) {
		return malloc(size);
	}
	if(!size) {
		free(ptr);
	}

	block_meta_t *p = (block_meta_t *)ptr;
	p--;
	KASSERT(p->magic_head == MAGIC_HEAD);
	KASSERT(p->magic_end == MAGIC_END);

	n = malloc(size);
	memcpy(n, ptr, size < p->size ? size : p->size);
	free(ptr);
	return n;
}

// Allocate a nbytes of memory, multiple of PAGE_SIZE, PAGE_SIZE aligned
// to be tested more!!!
void *malloc_page_aligned(unsigned int nbytes)
{

	block_meta_t *p, *n;
	void *m;
	KASSERT(nbytes); // > 0
	KASSERT(!(nbytes & 0xFFF)); // is PAGE_SIZE aligned?
	unsigned int p_size = nbytes + PAGE_SIZE + sizeof(block_meta_t);
	m = malloc(p_size);
	// kprintf("Alloc at: %p, p_size: %d\n", m, p_size);
	spin_lock(&kheap_lock);
	if(!((unsigned int)m & 0xFFF)) {
		//kprintf("ALIGNED?\n");
		return m;
	}
	p = (block_meta_t *) m - 1;
	n = (block_meta_t *)(((unsigned int)m & 0xFFFFF000) + PAGE_SIZE)-1;
	if((unsigned int)n < (unsigned int)m) {
		n = (block_meta_t *)(((unsigned int)m & 0xFFFFF000) + PAGE_SIZE * 2)-1;
	}
	if((unsigned int)n < (unsigned int)m) {
		kprintf("p:%p, m:%p, n:%p, n+1:%p, m+psize:%p\n", p,m,n,n+1,(unsigned int)m+p_size);
		panic("n < m\n");
	}
	if(((unsigned int)(n+1)+PAGE_SIZE) > ((unsigned int)m+p_size)) {
		kprintf("p:%p, m:%p, n:%p, n+1:%p, m+psize:%p\n", p,m,n,n+1,(unsigned int)m+p_size);
		panic("((n+1)+PAGE_SIZE) > m + p_size)\n");
	}

	n->magic_head = MAGIC_HEAD;
	n->magic_end = MAGIC_END;
	n->free = false;
	n->next = p->next;
	n->size = (unsigned int)m+p_size - (unsigned int)(n+1);
	p->size = (unsigned int)n-(unsigned int)m;
	p->next = n;
	spin_unlock(&kheap_lock);
	free(m);
	return n+1;
}

bool test_mem_1()
{
	unsigned int i;
	char *p;
	free(malloc(1));
	int *k;
	k = (int *)malloc(10000 * sizeof(int));
	//kprintf("Alloc\n");
	for (i = 4000; i < 5000; i++) {
		p = malloc(i);
		memset(p, 'A', i);
		k[i] = (unsigned int )p;
	}
	//kprintf("Freeing\n");
	for (i = 4000; i < 5000; i++) {
		free((void *)k[i]);
	}
	free(k);
	if(first_block->next == NULL && first_block->size == heap->end_addr-heap->start_addr - sizeof(block_meta_t)) {
		return true;
	}
	return false;
}


bool test_mem_2()
{
	void *i, *j, *p, *k;
	i = malloc(4000);
	p = malloc_page_aligned(2*PAGE_SIZE);
	memset(p, 'A', 2*PAGE_SIZE);
	k = malloc_page_aligned(PAGE_SIZE);
	memset(k, 'B', PAGE_SIZE);
	// debug_dump_list(first_block);
	free(p);
	free(k);
	j = malloc(10);
	free(i);
	free(j);
	if(first_block->next == NULL && first_block->size == heap->end_addr-heap->start_addr - sizeof(block_meta_t)) {
		return true;
	}
	return false;
}
bool test_mem_3() {
	unsigned int *p = malloc_page_aligned(PAGE_SIZE*3);
	virt_t *table;
	int dir_idx = ((unsigned int)p / PAGE_SIZE) / 1024;
	int tbl_idx = ((unsigned int)p / PAGE_SIZE) % 1024;
	table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
	if((table[tbl_idx] & 0xFFFFF000) == (unsigned int)virt_to_phys((virt_t)p)) {
		free(p);
		// kprintf("same: %p, %p\n", table[tbl_idx] & 0xFFFFF000, virt_to_phys(p));
		return true;
	}
	free(p);
	return false;
}

void heap_init() {

	//kprintf("In heap \n");
	KASSERT((HEAP_INITIAL_SIZE / PAGE_SIZE) > 0);
	if (!heap) {
		panic("Heap is not initialized\n");
	}
	init_first_block();

	// kprintf("test_mem_1 %s\n", test_mem_1() ? "passed":"FAILED");
	// kprintf("test_mem_2 %s\n", test_mem_2() ? "passed":"FAILED");
	// kprintf("test_mem_3 %s\n", test_mem_3() ? "passed":"FAILED");
	// // debug_dump_list(first_block);
	// // heap_dump();
	// malloc(12); malloc(10000);
	// heap_contract();
	// kprintf("test_mem_1 %s\n", test_mem_1() ? "passed":"FAILED");
	// kprintf("test_mem_2 %s\n", test_mem_2() ? "passed":"FAILED");
	// kprintf("test_mem_3 %s\n", test_mem_3() ? "passed":"FAILED");
	// heap_dump();
	// heap_contract();
	// heap_dump();
	return;
}
