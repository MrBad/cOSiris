// hacky thing

#include <stdio.h>

int vsnprintf(char *str, size_t size, const char  *fmt,  va_list ap)
{
    return vsprintf(str, fmt, ap);
}

