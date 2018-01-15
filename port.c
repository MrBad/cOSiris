#include <sys/types.h>
#include "i386.h"

int port_read(uint16_t port, void *buf, int count) {
	int i;
	uint16_t *dest = (uint16_t *) buf;
	for(i = 0; i < count; i++) {
		*dest++ = inw(port);
	}
	
	return i;
}

int port_write(uint16_t port, void *buf, int count)
{
	int i;
	uint16_t *src = (uint16_t *) buf;
	for(i = 0; i < count; i++) {
		outw(port, *src++);
	}

    return i;
}
