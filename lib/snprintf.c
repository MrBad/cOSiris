#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list args;
    int ret;
    va_start(args, fmt);
    ret = vsnprintf(str, size, fmt, args);
    va_end(args);

    return ret;
}

