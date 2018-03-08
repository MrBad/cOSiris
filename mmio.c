#include "mem.h"
#include "console.h"
#include "mmio.h"

/* Remapped Memory */
#define MMIO_START 0xA0000000
#define MMIO_END   0xB0000000

virt_t mmio_ptr;

/**
 * Remaps physical memory at address addr, having size size, 
 *  to arbitrary virtual memory in MMIO_START-MMIO_END region
 * TODO: rename func to map_to_any or something and also the MMIO region,
 * in order to be more generic.
 */
virt_t mmio_remap(phys_t addr, uint32_t size)
{
    uint32_t i;
    virt_t ret;

    if (size % PAGE_SIZE > 0)
        size = ((size / PAGE_SIZE) + 1) * PAGE_SIZE;
    if (mmio_ptr + size > MMIO_END)
        return 0;
    for (i = 0; i < size; i += PAGE_SIZE) {
        map(mmio_ptr + i, addr + i, P_PRESENT | P_READ_WRITE);
    }
    ret = mmio_ptr;
    mmio_ptr += i;

    return ret;
}

void mmio_init()
{
    mmio_ptr = MMIO_START;
}
