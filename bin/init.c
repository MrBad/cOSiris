#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "syscalls.h"

pid_t run_on_tty(char *prog, char **argv, char *tty_name)
{
    int i;
    pid_t pid;
    pid = fork();
    if (pid == 0) {
        for (i = 0; i < 3; i++)
            close(i);
        if (open(tty_name, O_RDWR, 0) < 0) {
            printf("Cannot open %s\n", tty_name);
            exit(1);
        }
        for (i = 0; i < 2; i++)
            dup(0);
        setsid();
        if (exec(prog, argv) < 0) {
            printf("Cannot exec %s on %s\n", prog, tty_name);
            exit(1);
        }
    }

    return pid;
}

struct rt {
    char *prog;
    char **argv;
    char *tty_name;
};

int main()
{
    pid_t pids[10], zpid;
    int status, i, found;
    char *argv[] = {0};

    struct rt ptbl[] = {
        {"/cosh", argv, "/dev/tty0",},
        {"/cosh", argv, "/dev/ttyS0"},
    };

    int ptbl_sz = ((int) sizeof(ptbl) / sizeof(*ptbl));

    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    printf("INIT started, in userland.\n"
            "Forking cosh, the interactive shell :D\n");

    for (i = 0; i < ptbl_sz; i++) {
        pids[i] = run_on_tty(ptbl[i].prog, ptbl[i].argv, ptbl[i].tty_name);
    }
    // We can be wake up because scheduler cannot run other process
    // We should ignore the -1 wait return
    for (;;) {
        if ((zpid = wait(&status)) < 0) {
            printf("init: error in wait\n");
            continue;
        }

        found = 0;
        for (i = 0; i < ptbl_sz; i++) {
            if (zpid == pids[i]) {
                printf("init: respawn %s on %s\n",
                        ptbl[i].prog, ptbl[i].tty_name);
                pids[i] = run_on_tty(ptbl[i].prog, ptbl[i].argv,
                        ptbl[i].tty_name);
                found = 1;
                break;
            }
        }
        if (!found)
            printf("init: RIP zombie pid: %d, status: %d\n", zpid, status);
    }

    return 0;
}
