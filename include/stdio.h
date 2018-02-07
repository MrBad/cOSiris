#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>

int vsprintf(char *str, const char *format, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int printf(char *fmt, ...);
void perror(const char *s);

#endif
