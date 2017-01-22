#include "gdt.h"
#include <string.h>

// 5 entries table, null descriptor, kernel code descriptor, kernel data descriptor,
// user code descriptor, user data descriptor //
struct gdt_entry gdt[6];
// ptr to this table //
struct gdt_ptr gdt_p;


void gdt_set_gate(int num, unsigned long base, unsigned long limit, unsigned char access, unsigned char gran)
{
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;
	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].granularity = ((limit >> 16) & 0x0F);
	gdt[num].granularity |= (gran & 0xF0);
	gdt[num].access = access;
}

void gdt_init(void)
{
	gdt_p.limit = (sizeof(struct gdt_entry) * 6) - 1;
	gdt_p.base = (unsigned int) &gdt;

	gdt_set_gate(0, 0, 0, 0, 0); // null descriptor

	// code segment base address 0x0, 4GB long, granulatie 4KB, 32 bits opcode,
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	// data segment base address 0x0, 4GB, granulatie 4KB,
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

	// user mode code segment
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
	// // user mode data segment
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

	gdt_flush();
	//tss_flush();
}

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

void set_kernel_stack(uint32_t stack)
{
	tss.esp0 = stack;
}
