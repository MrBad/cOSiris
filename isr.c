#include "isr.h"
#include "idt.h"
#include "console.h"


extern void page_fault(struct iregs *i);
extern void halt(void);

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


void isr_init(void){
	idt_set_gate(0, (unsigned)isr0, 0x08, 0x8E);
	idt_set_gate(1, (unsigned)isr1, 0x08, 0x8E);
	idt_set_gate(2, (unsigned)isr2, 0x08, 0x8E);
	idt_set_gate(3, (unsigned)isr3, 0x08, 0x8E);
	idt_set_gate(4, (unsigned)isr4, 0x08, 0x8E);
	idt_set_gate(5, (unsigned)isr5, 0x08, 0x8E);
	idt_set_gate(6, (unsigned)isr6, 0x08, 0x8E);
	idt_set_gate(7, (unsigned)isr7, 0x08, 0x8E);
	idt_set_gate(8, (unsigned)isr8, 0x08, 0x8E);
	idt_set_gate(9, (unsigned)isr9, 0x08, 0x8E);
	idt_set_gate(10, (unsigned)isr10, 0x08, 0x8E);
	idt_set_gate(11, (unsigned)isr11, 0x08, 0x8E);
	idt_set_gate(12, (unsigned)isr12, 0x08, 0x8E);
	idt_set_gate(13, (unsigned)isr13, 0x08, 0x8E);
	idt_set_gate(14, (unsigned)isr14, 0x08, 0x8E);
	idt_set_gate(15, (unsigned)isr15, 0x08, 0x8E);
	idt_set_gate(16, (unsigned)isr16, 0x08, 0x8E);
	idt_set_gate(17, (unsigned)isr17, 0x08, 0x8E);
	idt_set_gate(18, (unsigned)isr18, 0x08, 0x8E);
	idt_set_gate(19, (unsigned)isr19, 0x08, 0x8E);
	idt_set_gate(20, (unsigned)isr20, 0x08, 0x8E);
	idt_set_gate(21, (unsigned)isr21, 0x08, 0x8E);
	idt_set_gate(22, (unsigned)isr22, 0x08, 0x8E);
	idt_set_gate(23, (unsigned)isr23, 0x08, 0x8E);
	idt_set_gate(24, (unsigned)isr24, 0x08, 0x8E);
	idt_set_gate(25, (unsigned)isr25, 0x08, 0x8E);
	idt_set_gate(26, (unsigned)isr26, 0x08, 0x8E);
	idt_set_gate(27, (unsigned)isr27, 0x08, 0x8E);
	idt_set_gate(28, (unsigned)isr28, 0x08, 0x8E);
	idt_set_gate(29, (unsigned)isr29, 0x08, 0x8E);
	idt_set_gate(30, (unsigned)isr30, 0x08, 0x8E);
	idt_set_gate(31, (unsigned)isr31, 0x08, 0x8E);

}

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

void fault_handler(struct iregs *r) {
	if(r->int_no < 32) {
		if(r->int_no == 14) {
			page_fault(r);
			return;
		} else { 
			kprintf("%s Exception\n", exception_msgs[r->int_no]);
		}
		kprintf("  ds:  0x%-8X  es:  0x%-8X  fs:   0x%-8X  gs:   0x%-8X\n", r->ds, r->es, r->fs, r->gs);
		kprintf("  edi: 0x%-8X  esi: 0x%-8X  ebp:  0x%-8X  esp:  0x%-8X\n", r->edi, r->esi, r->ebp, r->esp);
		kprintf("  eax: 0x%-8X  ebx: 0x%-8X  ecx:  0x%-8X  edx:  0x%-8X\n", r->eax, r->ebx, r->ecx, r->edx);
		kprintf("  cs:  0x%-8X  eip: 0x%-8X  flgs: 0x%-8X  uesp: 0x%-8X\n", r->cs, r->eip, r->eflags, r->useresp);
		kprintf("  ss:  0x%-8X  errcode: %-8X\n", r->ss, r->err_code);
		halt();
	}
}
