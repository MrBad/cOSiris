#ifndef _MMIO_H
#define _MMIO_H

/* Remapped Memory */
#define MMIO_START 0xA0000000
#define MMIO_END   0xB0000000

virt_t mmio_remap(phys_t addr, uint32_t size);
void mmio_init();

#endif
