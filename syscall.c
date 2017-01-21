#include "isr.h"
#include "console.h"
#include "syscall.h"

void switch_to_user_mode()
{

}

unsigned int syscall_handler(struct iregs *r)
{
	kprintf("eax is: %p\n", r->eax);
	kprintf("ebx is: %p\n", r->ebx);
	kprintf("ecx is: %p\n", r->ecx);
	return 0;
}

void syscall_init()
{
	kprintf("Syscall Init\n");
	isr_install_handler(0x80, &syscall_handler);
}
