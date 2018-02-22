#include <stdio.h>

int main()
{
    printf("testing\n");
    
    __asm("int $3");

    printf("OK\n");
}
