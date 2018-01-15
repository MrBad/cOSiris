#include <string.h>
#include "console.h"
#include "idt.h"
#include "isr.h"
#include "i386.h"

extern int page_fault(struct iregs *i);

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr128();

/**
 * Global isr routines table
 */
void *isr_routines[256];

char *exception_msgs[] = {
    "Division By zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check (486+)",
    "Machine Check (Pentium/586+)",
    "Reserved Exceptions 19",
    "Reserved Exceptions 20",
    "Reserved Exceptions 21",
    "Reserved Exceptions 22",
    "Reserved Exceptions 23",
    "Reserved Exceptions 24",
    "Reserved Exceptions 25",
    "Reserved Exceptions 26",
    "Reserved Exceptions 27",
    "Reserved Exceptions 28",
    "Reserved Exceptions 29",
    "Reserved Exceptions 30",
    "Reserved Exceptions 31",
};

void isr_install_handler(uint8_t isr, isr_handler_t handler)
{
    isr_routines[isr] = handler;
}

void isr_uninstall_handler(uint8_t isr)
{
    isr_routines[isr] = 0;
}

static void fault_handler(struct iregs *r)
{
    int ret = 0;
    if (r->int_no < 32) {
        if (r->int_no == 14) {
            ret = page_fault(r);
            if (ret) {
                return;
            }
        } else {
            kprintf("%s Exception\n", exception_msgs[r->int_no]);
        }
        kprintf("  ds:  0x%-8X  es:  0x%-8X  fs:   0x%-8X  gs:   0x%-8X\n"
                "  edi: 0x%-8X  esi: 0x%-8X  ebp:  0x%-8X  esp:  0x%-8X\n"
                "  eax: 0x%-8X  ebx: 0x%-8X  ecx:  0x%-8X  edx:  0x%-8X\n"
                "  cs:  0x%-8X  eip: 0x%-8X  flgs: 0x%-8X  uesp: 0x%-8X\n"
                "  ss:  0x%-8X  errcode: %-8X\n",
                r->ds, r->es, r->fs, r->gs,
                r->edi, r->esi, r->ebp, r->esp,
                r->eax, r->ebx, r->ecx, r->edx,
                r->cs, r->eip, r->eflags, r->useresp,
                r->ss, r->err_code
               );
        halt();
    }
}

/**
 * Called from isr_common, from ASM
 */
uint32_t isr_handler(struct iregs *r)
{
    uint8_t int_no = 0xFF & r->int_no;
    isr_handler_t handler = isr_routines[int_no];
    if (handler) {
        return handler(r);
    }
    else {
        fault_handler(r);
        return 0;
    }
}

void isr_init() 
{
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    idt_set_gate(128, (unsigned)isr128, 0x08, 0xee); // EE - DPL3

    memset(isr_routines, 0, sizeof(isr_routines));
}

