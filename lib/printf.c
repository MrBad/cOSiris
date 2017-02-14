#include <stdarg.h>
#include <string.h>

// void print(char *str);

int printf(char * fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	buf[1023]=0;

	write(1, buf, strlen(buf)); // for now...

	return 0;
}
