#ifndef _X86_H
#define _X86_H

#include <sys/types.h>

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);

int port_read(uint16_t port, void *buf, int count);
int port_write(uint16_t port, void *buf, int count);


// pause interrupts
#define cli() __asm__ ("cli"::)
// restore interrupts
#define sti() __asm__ ("sti"::)
// no operation
#define nop() __asm__ ("nop"::)
// halt
#define hlt() __asm__ ("hlt"::)

extern void bochs_break();

typedef unsigned int spin_lock_t;
extern int spin_lock(spin_lock_t *lock);
// extern void spin_unlock(spin_lock_t *lock);
#define spin_unlock(addr) (*(addr)=0);
extern void halt();


#endif
