/**
 * A simple daemon program, for testing signals and kill
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include "syscalls.h"

int iterations = 2;
void sig_clean(int signum)
{
    char buf[512];
    int fd;

    fd = open("daemon.log", O_WRONLY|O_CREAT|O_APPEND, 644);
    if (fd > 0) {
        snprintf(buf, sizeof(buf), "pid %d got signal %d\n", getpid(), signum);
        write(fd, buf, strlen(buf));
        close(fd);
    }
    iterations--;
    //exit(0);
}

int main()
{
    pid_t pid, c_pid;
    int status, wp;
    
    signal(SIGTERM, sig_clean);

    if ((pid = fork()) == 0) {
        if ((c_pid =  fork()) == 0) {
            printf("Grandson up\n");
            // After 2 iteration, return.
            while (iterations > 0)
                ;
        } else {
            // Son, wait for grandson to exit and only then exit
            wp = wait(&status);
            printf("son end: pid: %d, status: %d\n", wp, status);
            return 0;
        }
    } else if (pid > 0) {
        printf("forking child with pid %d\n", pid);
        return 0;
    } else {
        printf("Error forking child!\n");
        return 1;
    }
}
