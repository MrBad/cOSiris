#ifndef _X86_H
#define _X86_H

void outb(unsigned short _port, unsigned char _data);
void outportw(unsigned _port, unsigned _data);
unsigned char inb(unsigned short _port);
// restore interrupts
#define sti() __asm__ ("sti"::)
// pause interrupts
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)
#define hlt() __asm__ ("hlt"::)
extern void bochs_break();

typedef unsigned int spin_lock_t;
extern int spin_lock(spin_lock_t *lock);
// extern void spin_unlock(spin_lock_t *lock);
#define spin_unlock(addr) (*(addr)=0);
extern void halt();
#endif
