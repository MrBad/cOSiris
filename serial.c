#include "include/stdarg.h"
#include "serial.h"
#define PORT 0x3f8   /* COM1 */



int serial_received() {
	return inb(PORT + 5) & 1;
}

char read_serial() {
	while (serial_received() == 0);
	return inb(PORT);
}

int is_transmit_empty() {
	return inb(PORT + 5) & 0x20;
}

void serial_putc(char a) {
	while (is_transmit_empty() == 0);
	outb(PORT,a);
}
void write_serial(char *buf) {
	for (;;) {
		if(*buf == 0) {
			break;
		}
		serial_putc(*buf++);
	}
}

void serial_debug(char *fmt, ...) {
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	write_serial(buf);
	return;
}


void init_serial() {
	outb(PORT + 1, 0x00);    // Disable all interrupts
	outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
	outb(PORT + 1, 0x00);    //                  (hi byte)
	outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}