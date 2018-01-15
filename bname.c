#include <string.h>
#include "assert.h"
#include "bname.h"

void bname(char *path, char *dirname, char *basename)
{
    char *p;
    unsigned long pos;
    size_t len = strlen(path);
    for (p = path + len; p > path; p--)
        if (*p == '/')
            break;

    if (*p == '/')
        p++;

    pos = p - path;
    KASSERT(pos <= MAX_PATH_LEN);
    strncpy(dirname, path, pos);
    dirname[pos] = 0;
    if (pos > 1 && dirname[pos - 1] == '/')
        dirname[pos - 1] = 0;

    KASSERT(len - pos <= MAX_PATH_LEN);
    strncpy(basename, p, len - pos);
    basename[len - pos] = 0;
}
