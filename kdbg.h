#ifndef _KDBG_H
#define _KDBG_H

#define KDBG_TTY TTYS1
#define BREAK() __asm("int $3")

extern uint32_t kdbg_handler_enter(struct iregs *r);
void kdbg_handler();
int kdbg_init();

#endif

