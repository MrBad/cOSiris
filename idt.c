#include <string.h>
#include "i386.h"
#include "idt.h"

struct idt_entry idt[256];
struct idt_ptr idt_p;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].base_lo = base & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_init()
{
    idt_p.limit = sizeof(idt) - 1;
    idt_p.base = (unsigned int) &idt;
    memset(&idt, 0, sizeof(idt));
    idt_load();
}
