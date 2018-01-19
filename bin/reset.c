#include <stdlib.h>
#include "syscalls.h"

int main()
{
    char *buf = "\033c";
    write(1, buf, 3);
}
