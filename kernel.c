#include "kernel.h"
#include <types.h>
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

unsigned int initrd_location, initrd_end;
unsigned int stack_ptr, stack_size;

void list_root(fs_node_t *fs_root)
{
	// list the contents of //
	unsigned int i = 0;
	struct dirent *node = 0;
	while((node = readdir_fs(fs_root, i))) {
		kprintf("Found file %s\n", node->name);
		fs_node_t *fs_node = finddir_fs(fs_root, node->name);
		if(fs_node->flags & FS_DIRECTORY) {
			kprintf("\tDir %s\t\n", fs_node->name);
		} else {
			// char buff[256];
			// unsigned int size, offs = 0;
			kprintf("name: %s, inode: %d, length: %d\n", fs_node->name, fs_node->inode, fs_node->length);
			// do {
			// 	size = read_fs(fs_node, offs, 256, buff);
			// 	kprintf("%s", buff);
			// 	offs+=size;
			// } while(size > 0);
		}
		i++;
	}
}


void shutdown()
{
	char *c = "Shutdown";
	while (*c) { outb(0x8900, *c++); }
}


extern int get_flags();
extern unsigned int get_esp();

void test_user_mode();
void main(unsigned int magic, multiboot_header *mboot, unsigned int ssize, unsigned int sptr) {
	stack_ptr = sptr;
	stack_size = ssize;

	console_init();
	serial_init();

	multiboot_parse(magic, mboot);
	kprintf("cOSiris\n");

	get_kernel_info(&kinfo);
	kprintf("Kernel info:\n");
	kprintf("\tcode start: 0x%X, start entry: 0x%X, data start: 0x%X\n", kinfo.code, kinfo.start, kinfo.data);
	kprintf("\tbss start: 0x%X, end: 0x%X, total size: %i bytes\n", kinfo.bss, kinfo.end, kinfo.size);
	kprintf("\tstack: %p, size: %p\n", kinfo.stack, kinfo.stack_size);
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
		kprintf("Modules: %d\n", mboot->mods_count);
		initrd_location = *((unsigned int *) mboot->mods_addr);
		initrd_end = *(unsigned int *) (mboot->mods_addr + 4);
	}

	kprintf("Setup paging\n");
	mem_init(mboot);

	fs_root = initrd_init(initrd_location);
	zero_init();

	task_init();

	fs_node_t *fds[2];
	new_pipe(fds);
	open_fs(fds[0], PIPE_READ);
	open_fs(fds[1], PIPE_WRITE);
	char buffer1[256], buffer2[256];
	unsigned int w,r;

	pid_t pid = fork();

	if(pid == 0) {
		// open_fs(fds[0], PIPE_READ);
		close_fs(fds[1]);
		r = read_fs(fds[0], 0, 256, buffer2);
		kprintf("Read %d bytes from pipe, [%s]\n", r, buffer2);
		close_fs(fds[0]);
	}
	else {
		cli(); // damn, i need a spinlock /
		// open_fs(fds[1], PIPE_WRITE);
		close_fs(fds[0]);
		strcpy(buffer1, "Testing Pipes on fork()");
		w = write_fs(fds[1], 0, strlen(buffer1), buffer1);
		kprintf("Wrote %d bytes to pipe\n", w);
		close_fs(fds[1]);
		sti();
	}
	return;
	// halt();
	//
	// fs_node_t *fds[2];
	// kprintf("\nTesting pipes\n");
	// new_pipe(fds);
	// open_fs(fds[0], 1);
	// open_fs(fds[1], 2);
	// char buff[256];
	// char buff2[256];
	// unsigned int w,r;
	// strcpy(buff, "TESTING PIPES\n");
	// w = write_fs(fds[1], 0, strlen(buff), buff);
	// kprintf("write: %d bytes to pipe\n", w);
	//
	// r = read_fs(fds[0], 0, 256, buff2);
	// kprintf("read: %d bytes from pipe: %s\n", r, buff2);
	//
	// close_fs(fds[0]);
	// close_fs(fds[1]);

	syscall_init();

	exec_init();

	kprintf("Should not get here\n");
	return;
}
