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
#include "kbd.h"
#include "mem.h"
#include "kheap.h"
#include "vfs.h"
#include "initrd.h"
#include "serial.h"

ulong_t get_esp(void);
ulong_t get_ebp(void);

unsigned int initrd_location, initrd_end;

void main(unsigned int magic, multiboot_header *mboot) {

	console_init();
	serial_init();

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

	kprintf("Setup irq\n");
	irq_install();

	sti();

	kprintf("Setup timer\n");
	timer_install();

	kprintf("Setup keyboard\n");
	kbd_install();

	// find location of initial ramdisk //
	if (mboot->mods_count > 0) {
		initrd_location = *((unsigned int *) mboot->mods_addr);
		initrd_end = *(unsigned int *) (mboot->mods_addr + 4);
	}

	kprintf("Setup paging\n");
	mem_init(mboot);

	// call_test_sp(0xDEADC0DE);
	// kprintf("Setup heap\n");
	heap_init(mboot);
	return;
	kprintf("Initialise initrd\n");
	// Initialise the initial ramdisk, and set it as the filesystem root.

	kprintf("initrd %p - %p\n", initrd_location, initrd_end);
	unsigned int i;
	fs_node_t *fs_root = initrd_init(initrd_location);
	// list the contents of //
	i = 0;
	struct dirent *node = 0;
	while((node = readdir_fs(fs_root, i))) {
		kprintf("Found file %s\n", node->name);
		fs_node_t *fs_node = finddir_fs(fs_root, node->name);
		if(fs_node->flags & FS_DIRECTORY) {
			kprintf("\tDir %s\t\n", fs_node->name);
		} else {
			char buff[256];
			unsigned int size;
			size = read_fs(fs_node, 0, 256, buff);
			buff[size] = 0;
			kprintf("\t%s\n", buff);
		}
		i++;
	}

	kprintf("Press esc to exit.\n");

//	while(1) {
//		timer_wait(1000);
//		kprintf("+");
//	}
//	for(i=0;i<100;i++) {
//		timer_wait(1000);
//	}
	return;
}
