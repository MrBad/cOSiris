#include <stdio.h>

int sscanf(const char *str, const char *fmt, ...)
{
    va_list args;
    int ret;
    va_start(args, fmt);
    ret = vsscanf(str, fmt, args);
    va_end(args);

    return ret;
}

