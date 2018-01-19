#include "i386.h"
#include "idt.h"
#include "irq.h"

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

#define port_8259M 0x20	// programable interrupt controller (PIC) - Master port
#define port_8259S 0xA0	// PIC - Slave port

// interrupt table
void *irq_routines[16] = {0};

void irq_install_handler(uint8_t irq, irq_handler_t handler)
{
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(uint8_t irq)
{
    irq_routines[irq] = 0;
}

/**
 * Remap programmable interrupt controller (PIC)
 *   giving them specific vector offsets (0x20, 0x28)
 */
static void irq_remap()
{
    outb(port_8259M, 0x11); // init master
    outb(port_8259S, 0x11); // init slave

    outb(port_8259M+1, 0x20); // master base offset interrupt; 0x20, mapped up
                              // after the 32 entries in IDT, which are isrs
    outb(port_8259S+1, 0x28); // slave base offset interrupt; 0x20+8

    outb(port_8259M+1, 0x04); // tell the master we have a slave on irq2
    outb(port_8259S+1, 0x02); // tell the slave it's in cascade identity

    outb(port_8259M+1, 0x01); // mod 8086 for both
    outb(port_8259S+1, 0x01);

    outb(port_8259M+1, 0x0);  // mask ports
    outb(port_8259S+1, 0x0);
}

void irq_install()
{
    irq_remap();
    idt_set_gate(32, (uint32_t) irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t) irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t) irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t) irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t) irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t) irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t) irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t) irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t) irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t) irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t) irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t) irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t) irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t) irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t) irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t) irq15, 0x08, 0x8E);
}

/**
 * IRQ handler, called from irq_common, from ASM
 */
void irq_handler(struct iregs *r)
{
    if (r->int_no >= 0x28) {	
        // send end of interrupt, if irq is on slave
        outb(port_8259S, 0x20);
    }
    // send end of interrupt on master
    outb(port_8259M, 0x20);
    irq_handler_t handler = irq_routines[r->int_no - 32];
    if (handler) {
        handler(r);
    }
}
