#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

int fprintf(FILE *fp, const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    int n;
    va_start(args, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n < 0)
        return -1;

    if ((unsigned) n < sizeof(buf))
        n = fwrite(buf, 1, n, fp);
    else
        n = fwrite(buf, 1, sizeof(buf) - 1, fp);

    return n;
}

