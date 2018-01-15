#include <string.h>
#include "i386.h"
#include "gdt.h"

/**
 * 6 entries table, 
 * null descriptor, 
 * kernel code descriptor, 
 * kernel data descriptor,
 * user code descriptor, 
 * user data descriptor
 * tss entry
 */
struct gdt_entry gdt[6];

/**
 * ptr to this table
 */
struct gdt_ptr gdt_p;

/**
 * Sets the global descriptor table entry num
 */
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access,
        uint8_t gran)
{
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

/**
 * Sets the tss. 
 * Usually this will be the kernel task state segment, that it will be loaded
 *  when switching from ring 3 to ring 0.
 * All we use from it is the stack.
 */
void write_tss(uint32_t num, uint32_t ss0, uint32_t esp0)
{
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = base + sizeof(tss);
    gdt_set_gate(num, base, limit, 0xE9, 0x0);
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = ss0;
    tss.esp0 = esp0;
    tss.cs = 0x0b;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;
}

/**
 * Initialize the global descriptor table
 */
void gdt_init()
{
    gdt_p.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gdt_p.base = (uint32_t) &gdt;

    gdt_set_gate(0, 0, 0, 0, 0); // null descriptor

    // Code segment base address 0x0, 4GB long, granularity 4KB, 32 bits opcode
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // Data segment base address 0x0, 4GB, granularity 4KB.
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // User mode code segment.
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    // User mode data segment.
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush();
}