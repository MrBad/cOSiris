#ifndef _SERIAL_H
#define _SERIAL_H

int serial_getc(int tty_minor);
void serial_putc(char a, int tty_minor);
void serial_write(char *buf, int tty_minor);
void serial_debug(char *fmt, ...);
void serial_init();

#endif
