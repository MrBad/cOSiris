#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

int fprintf(FILE *fp, const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	buf[1023]=0;

	return fwrite(buf, 1, strlen(buf), fp);
}
