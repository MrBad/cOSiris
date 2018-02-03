#include <string.h>
#include <sys/types.h>

char *strstr(const char *str, const char *substr)
{
    const char *p = str;
    size_t len = strlen (substr);

    while ((p = strchr(p, substr[0])) != NULL) {
        if (strncmp(p, substr, len) == 0)
            return (char *) p;
         p++;
    }

    return NULL;
}

