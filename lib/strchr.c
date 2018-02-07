#include <sys/types.h>

char *strchr(const char *str, int c)
{
    for (; *str && *str != c; str++)
        ;
    
    if (c == 0 || *str)
        return (char *)str;

    return NULL;
}

