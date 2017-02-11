#include "kernel.h"
#include <sys/types.h>
#include <string.h>
#include "x86.h"
#include "console.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "timer.h"
#include "delay.h"
#include "kbd.h"
#include "mem.h"
#include "kheap.h"
#include "vfs.h"
#include "initrd.h"
#include "serial.h"
#include "task.h"
#include "syscall.h"
#include "zero.h"
#include "pipe.h"
#include "hd_queue.h"
#include "cofs.h"

unsigned int initrd_location, initrd_end;
unsigned int stack_ptr, stack_size;

void main(unsigned int magic, multiboot_header *mboot, unsigned int ssize, unsigned int sptr)
{
	stack_ptr = sptr;
	stack_size = ssize;

	serial_init();


	// multiboot_parse(magic, mboot);
	kprintf("cOSiris\n");

	get_kernel_info(&kinfo);
	kprintf("Kernel info:\n");
	kprintf("\tcode start: 0x%p, start entry: 0x%p, data start: 0x%p\n", kinfo.code, kinfo.start, kinfo.data);
	kprintf("\tbss start: 0x%p, end: 0x%p, total size: %d KB\n", kinfo.bss, kinfo.end, kinfo.size/1024);
	kprintf("\tstack: 0x%p, size: 0x%p\n", stack_ptr, stack_size);
	kprintf("Setup gdt entries\n");
	gdt_init();

	kprintf("Setup idt entries\n");
	idt_init();

	kprintf("Setup isr\n");
	isr_init();

	kprintf("Setup irq\n");
	irq_install();

	sti();

	kprintf("Setup timer\n");
	timer_install();

	kprintf("Calibrating delay loop: %d loops/ms\n", calibrate_delay_loop());

	kprintf("Setup keyboard\n");
	kbd_install();

	// find location of initial ramdisk //
	if (mboot->mods_count > 0) {
		initrd_location = *((unsigned int *) mboot->mods_addr);
		initrd_end = *(unsigned int *) (mboot->mods_addr + 4);
	}

	kprintf("Setup paging\n");
	mem_init(mboot);
	// fs_root = initrd_init(initrd_location);

	fs_root = cofs_init();
	// zero_init();
	console_init();
	task_init();
	syscall_init();
	sys_exec("/init", NULL);

	// ps();
	kprintf("Should not get here\n");
	return;
}
