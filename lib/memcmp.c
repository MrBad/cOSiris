#include <sys/types.h>

int memcmp(const void *s1, const void *s2, size_t n)
{
    unsigned int i;
    char *str1 = (char *) s1;
    char *str2 = (char *) s2;

    for (i = 0; i < n; i++)
        if (str1[i] != str2[i])
            return str1[i] - str2[i];
    
    return 0;
}

