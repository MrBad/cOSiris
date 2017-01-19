#ifndef _KHEAP_H
#define _KHEAP_H

#include "include/types.h"
#define HEAP_START          0xD0000000
#define HEAP_INITIAL_SIZE   0x00010000
#define HEAP_END            0xE0000000


typedef struct block_meta {
	unsigned int magic_head;
	unsigned int size;
	bool free;
	struct block_meta *next;
	unsigned int magic_end;
} block_meta_t;

typedef struct {
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int max_addr;
	bool supervisor;
	bool readonly;
} heap_t;

block_meta_t *first_block;
void heap_dump();
void debug_dump_list(block_meta_t *);


extern void heap_init();
extern heap_t *heap;
void *malloc(unsigned int nbytes);
void *calloc(unsigned int nbytes);
// Allocate a nbytes of memory, multiple of PAGE_SIZE, PAGE_SIZE aligned
void *malloc_page_aligned(unsigned int nbytes);
#endif
