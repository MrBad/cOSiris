#ifndef _SERIAL_H
#define _SERIAL_H

#include "x86.h"
void serial_write(char *buf);
void serial_debug(char *fmt, ...);
void serial_init();

#endif
