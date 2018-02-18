#include <stdlib.h> // self
#include <unistd.h> // sbrk
#include <assert.h>
#include <string.h>	// memset

#ifndef LOCK
#define LOCK(x) &x = 1;
#endif

#ifndef UNLOCK
#define UNLOCK(x) &x = 0;
#endif

#define MAGIC_HEAD              0xDEADC0DE
#define MAGIC_END               0xDEADBABA
#define PAGE_SIZE               0x1000
#define MIN_HEAP_CONTRACT_SIZE  PAGE_SIZE * 2

typedef struct block_meta {
    unsigned int magic_head;
    unsigned int size;
    bool free;
    struct block_meta * next;
    unsigned int magic_end;
} block_meta_t;

static block_meta_t *first_block;

static block_meta_t *init_first_block()
{
    void *start_addr;
    start_addr = sbrk(PAGE_SIZE);
    if(start_addr == (void *) -1) {
        return NULL;
    }
    // printf("Installing heap @%p, %08X\n", start_addr, PAGE_SIZE);
    first_block = (block_meta_t *) start_addr;
    first_block->free = true;
    first_block->magic_head = MAGIC_HEAD;
    first_block->magic_end = MAGIC_END;
    first_block->size = PAGE_SIZE - sizeof(block_meta_t);
    first_block->next = NULL;
    return first_block;
}

void *malloc(size_t size)
{
    //spin_lock(&kheap_lock);
    block_meta_t *p, *n;
    unsigned int next_size;
    char *c;
    if(size % 4 != 0)
        size = (size / 4 + 1) * 4; // align to 4 bytes
    if(first_block == NULL) {
        init_first_block();
    }

    assert(first_block->magic_head == MAGIC_HEAD);
    // assert(first_block->magic_end == MAGIC_END);
    p = first_block;
    unsigned int search_size = size + sizeof(block_meta_t);
    while (p) {
        if (!p->free) {
            p = p->next;
            continue;
        }
        // If last node, and we don't have required size, ask system for more
        if (p->next == NULL && p->size < search_size) {
            sbrk(PAGE_SIZE);
            p->size += PAGE_SIZE;
            continue;
        }
        // If not last node and we have the size, split it.
        else if(p->size >= search_size) {
            // printf("SPLIT; p: 0x%X orig_size: %i, nbytes: %i, meta: %i\n",
            //        p+1, p->size, size, sizeof(block_meta_t));
            assert(p->size >= size + sizeof(block_meta_t));
            // assert((unsigned int)(p+1) < heap->end_addr);
            c = (char *) p;
            next_size = p->size - size - sizeof(block_meta_t);
            n = p->next;
            p->size = size;
            p->free = false;
            p->next = (block_meta_t *) (c + sizeof(block_meta_t) + size);
            p->next->free = true;
            p->next->size = next_size;
            p->next->magic_head = MAGIC_HEAD;
            p->next->magic_end = MAGIC_END;
            p->next->next = n;
            // spin_unlock(&kheap_lock);
            return p + 1;
        }
        p = p->next;
    }
    return NULL;
}

static void heap_contract()
{
    // spin_lock(&kheap_lock);
    block_meta_t *p = first_block;
    unsigned int new_end;

    while (p->next) {
        p = p->next;
    }
    if(p->size > PAGE_SIZE) {
        unsigned int end_addr = (unsigned int)sbrk(0);
        assert((unsigned int)(p+1) + p->size == end_addr);
        new_end = ((unsigned int)(p+1) & 0xFFFFF000) + MIN_HEAP_CONTRACT_SIZE;
        p->size = new_end - (unsigned int)(p+1);
        sbrk(new_end - end_addr);
    }
    // spin_unlock(&kheap_lock);
}

static void dump_stack(void *p)
{
    unsigned int *k, i;

    printf("Stack:\n");
    for (k = (unsigned int *)&p - 1, i = 0; i < 16; k++, i++) {
        printf("0x%8X: 0x%8X \n", k, *k);
    }
}

void free(void *ptr)
{
    block_meta_t *p, *prev = NULL;
    // spin_lock(&kheap_lock);
    if (!ptr)
        return;
    p = first_block;
    for (; ;) {
        if (p + 1 == ptr) {
            assert(p->magic_head == MAGIC_HEAD);
            assert(p->magic_end == MAGIC_END);
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
            // Means we are at the last one, or something terrible
            // happent to heap. If magics are correct, it's a double free ptr.
            if (p->free && p->magic_head == MAGIC_HEAD
                    && p->magic_end == MAGIC_END && p->size > 0) {
                printf("Double free or corruption: %p, not found\n", ptr);
                dump_stack(ptr);
                abort();
            }
        }
        prev = p;
        p = p->next;
    }
    // spin_unlock(&kheap_lock);
    heap_contract();
}

void *calloc(size_t nmemb, size_t size)
{
    unsigned int nbytes = nmemb * size;
    void *p = malloc(nbytes);
    memset(p, 0, nbytes);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    void *n;
    if (!ptr) {
        return malloc(size);
    }
    if (!size) {
        free(ptr);
    }
    block_meta_t *p = (block_meta_t *)ptr;
    p--;
    assert(p->magic_head == MAGIC_HEAD);
    assert(p->magic_end == MAGIC_END);

    n = malloc(size);
    memcpy(n, ptr, size < p->size ? size : p->size);
    free(ptr);
    return n;
}

