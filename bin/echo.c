#include <stdio.h>
#include "syscalls.h"

int main(int argc, char *argv[])
{
    int i;
    printf("");
    for (i = 1; i < argc; i++) {
        printf("%s%c", argv[i], i + 1 < argc ? ' ' : '\n');
    }
}
