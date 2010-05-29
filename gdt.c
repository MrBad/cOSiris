#include "gdt.h"

// tabela cu 3 entries, null descriptor, code descriptor, data descriptor //
struct gdt_entry gdt[3];
// pointer catre aceasta tabela //
struct gdt_ptr gdt_p;

// declarata in startup.asm - folosita pt a reincarca noii registrii de segment //
extern void gdt_flush(void);

void gdt_set_gate(int num, unsigned long base, unsigned long limit, unsigned char access, unsigned char gran) {
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;
	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].granularity = ((limit >> 16) & 0x0F);
	gdt[num].granularity |= (gran & 0xF0);
	gdt[num].access = access;
}

void gdt_init(void) {
	gdt_p.limit = (sizeof(struct gdt_entry) * 3) - 1;
	gdt_p.base = (unsigned int) &gdt;
	
	gdt_set_gate(0,0,0,0,0); // null descriptor
	// code segment base address 0x0, 4GB long, granulatie 4KB, 32 bits opcode, 
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	// data segment base address 0x0, 4GB, granulatie 4KB, 
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
	gdt_flush();
}