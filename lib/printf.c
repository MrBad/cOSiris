#include <stdarg.h>
void print(char *str);

int printf(char * fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	print(buf); // for now...

	return 0;
}
