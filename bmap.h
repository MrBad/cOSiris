#ifndef _BMAP_H
#define _BMAP_H

#include <sys/types.h>

typedef struct bmap {
    int size;
    int last;
    spin_lock_t lock;
    uint32_t data[];
} bmap_t;

bmap_t *bmap_alloc(uint32_t size);
int bmap_get_free(bmap_t *bmap);
int bmap_free(bmap_t *bmap, int bit);

#endif

