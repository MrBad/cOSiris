#include <sys/types.h>
#include <string.h>
#include "i386.h"
#include "console.h"
#include "serial.h"
#include "mem.h"
#include "kheap.h"
#include "assert.h"

#define MAGIC_HEAD 0xDEADC0DE
#define MAGIC_END 0xDEADBABA

spin_lock_t kheap_lock;
extern bool heap_active;

heap_t myheap = {0};
heap_t *kernel_heap = &myheap;

void heap_dump(heap_t *h)
{
    kprintf("HEAP\tstart_addr: %p, end_addr: %p, max_addr: %p, \n",
            h->start_addr, h->end_addr, h->max_addr);
}

/**
 * Initialize kernel heap
 */
void heap_init()
{
    kernel_heap->start_addr = HEAP_START;
    kernel_heap->end_addr = HEAP_START;
    kernel_heap->max_addr = HEAP_END;
    kernel_heap->readonly = false;
    kernel_heap->supervisor = true;
    if (!kernel_heap) {
        panic("Heap is not initialized\n");
    }
    return;
}
