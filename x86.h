void outb(unsigned short _port, unsigned char _data);
void outportw(unsigned _port, unsigned _data);
unsigned char inb(unsigned short _port);
#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)
