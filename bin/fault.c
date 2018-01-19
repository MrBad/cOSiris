#include <stdio.h>

int main()
{
    printf("Going to create a fault\n");
    unsigned int *addr = (unsigned int *) 0xABADC0DE, read;
    read = *addr;
    printf("We read the var, should not happen!\n");
    read = 0x0BADC0DE;
    *addr = read;

    return 0;
}
