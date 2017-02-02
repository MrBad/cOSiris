#include <sys/types.h>
#include "isr.h"
#include "console.h"
#include "syscall.h"
#include "task.h"
#include "x86.h"
#include "mem.h"


extern void print_int(unsigned int x);
void exit(int status) {
	task_exit(status);
}
void print(char *buf){
	console_write(buf);
}
static void *syscalls[] = {
	&console_write,
	&print_int,
	&fork,
	&task_wait,
	&task_exit,
	&getpid,
	&ps,
	&sys_sbrk,
};
static unsigned int num_syscalls;

unsigned int syscall_handler(struct iregs *r)
{
	// cli();
	if(r->eax >= num_syscalls) {
		kprintf("No such syscall: %p\n", r->eax);
		return 0;
	} else {
		// kprintf("kernel: syscall: %d, %p\n", r->eax, r->ebx);
	}
	// kprintf("eax: %p, ebx: %p\n", r->eax, r->ebx);
	void *func = syscalls[r->eax];
	int ret;
	asm volatile(" \
		push %1;  \
		push %2;  \
		push %3;  \
		push %4;  \
		push %5;  \
		call *%6; \
		pop %%ebx; \
		pop %%ebx; \
		pop %%ebx; \
		pop %%ebx; \
		pop %%ebx; \
		"
	: "=r"(ret)
	:  "r"(r->edi), "r"(r->esi), "r"(r->edx), "r"(r->ecx), "r"(r->ebx), "r"(func));
//	call_sys(func, r->ebx, r->ecx, r->edx, r->esi, r->edi);
	r->eax = ret;
	// kprintf("syscall ok\n");
	return ret;
}

unsigned int syscall_handler(struct iregs *r);
void syscall_init()
{
	kprintf("Syscall Init\n");
	isr_install_handler(0x80, &syscall_handler);
	num_syscalls = sizeof(syscalls) / sizeof(void *);
}
