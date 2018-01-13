#ifndef _SERIAL_H
#define _SERIAL_H

void serial_putc(char a);
void serial_write(char *buf);
void serial_debug(char *fmt, ...);
void serial_init();

#endif
