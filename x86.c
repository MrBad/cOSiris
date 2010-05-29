//extern void outb(unsigned short port, unsigned char value);

void outb(unsigned short _port, unsigned char _data)
{
   __asm__ ("out %%al, %%dx" : : "a" (_data), "d" (_port));
}

void outportw(unsigned _port, unsigned _data)
{
	__asm__ ("out %%ax, %%dx" : : "a" (_data), "d" (_port));
}


unsigned char inb(unsigned short _port)
{
	unsigned char result;
	__asm__ ("in %%dx, %%al" : "=a" (result) : "d" (_port));
	return result;
}

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)
