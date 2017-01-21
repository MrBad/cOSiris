#ifndef _ISR_H
#define _ISR_H

struct iregs
{
	unsigned int gs, fs, es, ds;      /* pushed the segs last */
	unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* pushed by 'pusha' */
	unsigned int int_no, err_code;    /* our 'push byte #' and ecodes do this */
	unsigned int eip, cs, eflags, useresp, ss;   /* pushed by the processor automatically */
};


// extern void isr_handler();
void isr_init();
void isr_install_handler(int isr, unsigned int (*handler)(struct iregs *r));
void isr_uninstall_handler(int isr);

#endif
