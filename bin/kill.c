#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        printf("Usage: %s pid...\n", argv[0]);
        return 1;
    }
    for (i = 1; i < argc; i++) {
        printf("Sending SIGTERM to %d\n", atoi(argv[i]));
        kill(atoi(argv[i]), SIGTERM);
    }
    return 0;
}
