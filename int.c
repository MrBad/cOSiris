#include <string.h>
#include "console.h"
#include "idt.h"
#include "int.h"
#include "irq.h"
#include "i386.h"
#include "task.h"

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
 * Global int routines table
 */
void *int_routines[256];

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

void int_install_handler(uint8_t int_no, int_handler_t handler)
{
    int_routines[int_no] = handler;
}

void int_uninstall_handler(uint8_t int_no)
{
    int_routines[int_no] = 0;
}

void print_registers(struct iregs *r)
{

    kprintf(
            "   eax: %#010x       ecx: %#010x     edx: %#010x    ebx: %#010x\n"
            "   esp: %#010x       ebp: %#010x     esi: %#010x    edi: %#010x\n"
            "   eip: %#010x    eflags: %#010x      cs: %#010x     ss: %#010x\n"
            "    ds: %#010x        es: %#010x      fs: %#010x     gs: %#010x\n"
            "  uesp: %#010x   errcode: %#010x     pid: %d\n",
            r->eax, r->ecx, r->edx, r->ebx,
            r->esp, r->ebp, r->esi, r->edi,
            r->eip, r->eflags, r->cs, r->ss,
            r->ds, r->es, r->fs, r->gs, r->useresp, r->err_code,
            current_task ? current_task->pid : 0);
}

static void fault_handler(struct iregs *r)
{
    int ret = 0;
    if (r->int_no < 32) {
        if (r->int_no == 14) {
            if ((ret = page_fault(r)))
                return;
        } else {
            kprintf("%s Exception\n", exception_msgs[r->int_no]);
        }
        print_registers(r);
        if (r->int_no == 3) {
            return;
        }
        halt();
    }
}

/**
 * ISR handler
 */
uint32_t isr_handler(struct iregs *r)
{
    int_handler_t handler;
    int ret = 0;
    uint8_t int_no = 0xFF & r->int_no;

    if (int_no == 128)
        sti();

    handler = int_routines[int_no];
    if (handler)
        ret = handler(r);
    else
        fault_handler(r);

    return ret;
}

/**
 * Interrupt handler. This is the entry point from asm isrXX or irqXX
 */
uint32_t int_handler(iregs_t *regs)
{
    uint32_t u_esp;
    int ret = 0;
    uint8_t int_no = regs->int_no;

    if (current_task) {
        u_esp = current_task->regs.useresp;
        memcpy(&current_task->regs, regs, sizeof(*regs));
        /* Keep the user esp from last interrupt from ring 3 */
        if (regs->useresp == 0) {
            current_task->regs.useresp = u_esp;
        }
    }
    if (int_no > 31 && int_no < 48) {
        irq_handler(regs);
    } else {
        ret = isr_handler(regs);
    }
    return ret;
}

void int_init() 
{
    idt_set_gate(0,  (uint32_t) isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t) isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t) isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t) isr3,  0x08, 0xEE);
    idt_set_gate(4,  (uint32_t) isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t) isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t) isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t) isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t) isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t) isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t) isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t) isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t) isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t) isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t) isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t) isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t) isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t) isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t) isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t) isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t) isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t) isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t) isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t) isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t) isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t) isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t) isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t) isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t) isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t) isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t) isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t) isr31, 0x08, 0x8E);

    idt_set_gate(128, (unsigned) isr128, 0x08, 0xee); // EE - DPL3

    memset(int_routines, 0, sizeof(int_routines));
}

