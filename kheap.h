#ifndef _KHEAP_H
#define _KHEAP_H

#include <sys/types.h>

#define HEAP_START          0xD0000000  // Where kernel heap starts.
#define HEAP_END            0xE0000000  // Kernel heap maximum address

#define UHEAP_START         0x20000000  // Where user/ring3 heap starts
#define UHEAP_END           0x30000000  // Where user heap ends

/**
 * Structure of a heap
 */
typedef struct {
    unsigned int start_addr;
    unsigned int end_addr;
    unsigned int max_addr;
    bool supervisor;
    bool readonly;
} heap_t;

/**
 * Dumps the heap
 */
void heap_dump(heap_t *h);

/**
 * Initialize kernel heap
 */
void heap_init();

extern heap_t *kernel_heap;

#endif
