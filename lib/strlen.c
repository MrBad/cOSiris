#include <string.h>

size_t strlen(const char *s)
{
    size_t i = 0;
    if (s) {
        while (*s++ != 0)
            i++;
    }

    return i;
}

