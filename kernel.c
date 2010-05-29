#include "kernel.h"
#include <types.h>
#include "x86.h"
#include "console.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "timer.h"
#include "mem.h"
ulong_t get_esp(void);
ulong_t get_ebp(void);


void main(unsigned int magic, multiboot_header *mboot)
{
	console_init();
	multiboot_parse(magic, mboot);
	kprintf("cOSiris\n");

	get_kernel_info(&kinfo);
	kprintf("Kernel info:\n");
	kprintf("\tcode start: 0x%X, start entry: 0x%X, data start: 0x%X\n", kinfo.code, kinfo.start, kinfo.data);
	kprintf("\tbss start: 0x%X, end: 0x%X, total size: %i bytes\n", kinfo.bss, kinfo.end, kinfo.size);

	kprintf("Setup gdt entries\n");
	gdt_init();


	kprintf("Setup idt entries\n");
	idt_init();

	kprintf("Setup isr\n");
	isr_init();

	kprintf("Setup interrupts\n");
	irq_install();
	
	sti();
	kprintf("Setup paging\n");
	mem_init(mboot);


	kprintf("Setup timer\n");
	timer_install();

	while(1) {
		timer_wait(1000);
		kprintf("+");
	}
	return;
}
