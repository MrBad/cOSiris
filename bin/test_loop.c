#include <stdio.h>
#include "syscalls.h"

int main()
{
    char buf[16];
    printf("Going into endless loop, are you sure (y/n)?\n");
    read(0, buf, sizeof(buf)-1);
    if (buf[0] == 'y' || buf[0] == 'Y') {
        while(1)
            ;
    }

    return 0;
}
