//extern void outb(unsigned short port, unsigned char value);
#include <sys/types.h>

uint8_t inb(uint16_t port)
{
	unsigned char result;
	asm("in %%dx, %%al" : "=a" (result) : "d" (port));
	return result;
}

uint16_t inw(uint16_t port)
{
	unsigned short result;
	asm("in %%dx, %%ax" : "=a" (result) : "d" (port));
	return result;
}

void outb(uint16_t port, uint8_t data)
{
	asm("out %%al, %%dx" : : "a" (data), "d" (port));
}

void outw(uint16_t port, uint16_t data)
{
	asm("out %%ax, %%dx" : : "a" (data), "d" (port));
}

// reads count words from port into buf
int port_read(uint16_t port, void *buf, int count) {
	int i;
	uint16_t *dest = (uint16_t *) buf;
	for(i = 0; i < count; i++) {
		*dest++ = inw(port);
	}
	return i;
}
// writes count words from buf to port
int port_write(uint16_t port, void *buf, int count)
{
	int i;
	uint16_t *src = (uint16_t *) buf;
	for(i = 0; i < count; i++) {
		outw(port, *src++);
	}
	return i;
}
