#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 512

int printf(char * fmt, ...)
{
    char buf[BUF_SIZE];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, BUF_SIZE, fmt, args);
    va_end(args);

    if (n < BUF_SIZE)
        n = write(1, buf, n);
    else
        n = write(1, buf, BUF_SIZE - 1);

    return n;
}

