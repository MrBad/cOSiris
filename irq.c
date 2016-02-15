#include "x86.h" // outb
#include "isr.h" // iregs struct
#include "idt.h" // idt_set_gate


extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);


#define port_8259M 0x20	// programable interrupt controller (PIC) - Master port
#define port_8259S 0xA0	// PIC - Slave port

// pointer catre rutinile de tratare a intreruperilor //
void *irq_routines[16] = {
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0
};

void irq_install_handler(int irq, void (*handler)(struct iregs *r)) {
	irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq){
	irq_routines[irq] = 0;
}


void irq_remap(void) {
	outb(port_8259M, 0x11); // init master
	outb(port_8259S, 0x11); // init slave
	
	outb(port_8259M+1, 0x20);	// intreruperea de baza pt master - 0x20 (remapate dupa primele 32 entries in IDT, care sunt isrs)
	outb(port_8259S+1, 0x28);	// intreruperea de baza pt slave - 0x20+8

	outb(port_8259M+1, 0x04);	// primul chip e master
	outb(port_8259S+1, 0x02);	// al doilea chip e slave
	
	outb(port_8259M+1, 0x01);	// mod 8086 pt amandoua
	outb(port_8259S+1, 0x01);

	outb(port_8259M+1, 0x0);	// mascam porturile
	outb(port_8259S+1, 0x0);
}

void irq_install(void){
	irq_remap();
	idt_set_gate(32, (unsigned) irq0, 0x08, 0x8E);
	idt_set_gate(33, (unsigned) irq1, 0x08, 0x8E);
	idt_set_gate(34, (unsigned) irq2, 0x08, 0x8E);
	idt_set_gate(35, (unsigned) irq3, 0x08, 0x8E);
	idt_set_gate(36, (unsigned) irq4, 0x08, 0x8E);
	idt_set_gate(37, (unsigned) irq5, 0x08, 0x8E);
	idt_set_gate(38, (unsigned) irq6, 0x08, 0x8E);
	idt_set_gate(39, (unsigned) irq7, 0x08, 0x8E);
	idt_set_gate(40, (unsigned) irq8, 0x08, 0x8E);
	idt_set_gate(41, (unsigned) irq9, 0x08, 0x8E);
	idt_set_gate(42, (unsigned) irq10, 0x08, 0x8E);
	idt_set_gate(43, (unsigned) irq11, 0x08, 0x8E);
	idt_set_gate(44, (unsigned) irq12, 0x08, 0x8E);
	idt_set_gate(45, (unsigned) irq13, 0x08, 0x8E);
	idt_set_gate(46, (unsigned) irq14, 0x08, 0x8E);
	idt_set_gate(47, (unsigned) irq15, 0x08, 0x8E);
}

void irq_handler(struct iregs *r) {
	// a blank function pointer //
	void (*handler) (struct iregs *r);
	handler = irq_routines[r->int_no - 32];
	if(handler) {
		handler(r);
	}
	if(r->int_no >= 0x28) {	// daca e intrerupere pe slave, send end of interrupt
		outb(port_8259S, 0x20); 
	}
	
	// trimite end of interrupt pe master indiferent (sclavul e legat la master irq2, triggereaza ambele)
//	kprintf("x%X ", &handler);
	outb(port_8259M, 0x20);
}