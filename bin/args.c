#include <stdio.h>

int main(int argc, char *argv[])
{
    int i;

    printf("Got %d argc\n-\n", argc);
    for (i = 0; i < argc + 1; i++) {
        printf("%d: %s\n", i, argv[i]);
    }

    return 0;
}
