#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int hits = 3;

void dump_stack(int p)
{
    unsigned int *i, j;

    for (i = ((unsigned int *)&p)-1, j = 0; j < 32; i++, j++) {
        printf("0x%X: 0x%X\n", i, *i);
        if (*i == 0xC0DEBABE || *i == 0xC0DEDAC0)
            break;
    }
}

void my_int(int signum)
{
    printf("\nGot signal %d\nDumping stack, hits left: %d\n", 
            signum, hits);
    dump_stack(0xDADADADA);
    if (--hits == 0)
        exit(0);
}

void loop()
{
    while (1)
        ;
}

int main()
{
    printf("Press CTRL+X 3 times to kill me.\n");
    signal(SIGINT, my_int);
    loop();
}
