#include <stdlib.h>
#include "assert.h"
#include "console.h"
#include "bmap.h"

bmap_t *bmap_alloc(uint32_t size)
{
    bmap_t *bmap = NULL;
    if (!(bmap = calloc(1, sizeof(*bmap) + size / 8)))
        kprintf("Could not allocate bitmap\n");
    else
        bmap->size = size;

    return bmap;
}

static int bmap_scan_range(bmap_t *bmap, int min, int max)
{
    int i, j;

    KASSERT(bmap->lock);

    for (i = min / 32; i < max / 32; i++) {
        if (bmap->data[i] != 0xFFFFFFFF) {
            for (j = 0; j < 32; j++) {
                if (((bmap->data[i] >> j) & 0x1) == 0) {
                    bmap->data[i] |= 1 << j;
                    bmap->last = i * 32 + j;
                    return bmap->last;
                }
            }
        }
    }

    return -1;
}

int bmap_get_free(bmap_t *bmap) 
{
    int bit = -1;

    spin_lock(&bmap->lock);
    if ((bit = bmap_scan_range(bmap, bmap->last, bmap->size)) >= 0) {
        spin_unlock(&bmap->lock);
        return bit;
    }
    if ((bit = bmap_scan_range(bmap, 0, bmap->last)) >= 0) {
        spin_unlock(&bmap->lock);
        return bit;
    }
    spin_unlock(&bmap->lock);

    return bit;
}

int bmap_free(bmap_t *bmap, int bit)
{
    uint32_t *ptr;

    if (bit >= bmap->size)
        return -1;
    spin_lock(&bmap->lock);
    ptr = &bmap->data[bit / 32];
    *ptr &= ~(1 << (bit % 32));
    bmap->last = bit;
    spin_unlock(&bmap->lock);

    return 0;
}

