#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"

int main()
{
    pid_t pid, zpid;
    int status;
    char *argv[] = {0};

    printf("INIT started, in userland.\n"
            "Forking cosh, the interactive shell :D\n");

    while (1) {

        pid = fork();

        if (pid == 0) {
            if (exec("/cosh", argv) < 0) {
                printf("Cannot load cosh\n");
                return 1;
            }
        }
        // We can be wake up because scheduler cannot run other process
        // We should ignore the -1 wait return
        for (;;) {
            if ((zpid = wait(&status)) < 0)
                continue;
            if (zpid == pid) {
                printf("init: cosh pid: %d died with status: %d\n"
                        , zpid, status);
                break;
            } else {
                printf("init: RIP zombie pid: %d, status: %d\n", zpid, status);
            }
        }
    }
    return 0;
}
