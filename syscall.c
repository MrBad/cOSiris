#include <sys/types.h>
#include <string.h>
#include "isr.h"
#include "console.h"
#include "syscall.h"
#include "task.h"
#include "x86.h"
#include "mem.h"
#include "sysfile.h"
#include "cofs.h"

void exit(int status) {
	task_exit(status);
}
// void print(char *buf){
	// console_write(NULL, 0, strlen(buf), buf);
// }


void lstree_null() {
	lstree(NULL, 0);
}
static void *syscalls[] = {
	&lstree_null,

	&fork,
	&task_wait,
	&task_exit,
	&getpid,
	&sys_exec,
	&ps,

	&sys_sbrk,

	&sys_open,
	&sys_close,
	&sys_stat,
	&sys_fstat,
	&sys_read,
	&sys_write,
	&sys_chdir,
	&sys_chroot,
	&sys_chmod,
	&sys_chown,
	&sys_mkdir,
	&cofs_dump_cache,
	&sys_isatty,
	&sys_lseek,
	&sys_opendir,
	&sys_closedir,
	&sys_readdir,
	&sys_lstat,
	&sys_readlink,
	&sys_getcwd,
	&sys_unlink,
};
static unsigned int num_syscalls;

unsigned int syscall_handler(struct iregs *r)
{
	// cli();
	if(r->eax >= num_syscalls) {
		kprintf("No such syscall: %d\n", r->eax);
		r->eax = 0;
		return r->eax;
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
