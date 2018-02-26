#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

enum {
    STDIN_FILENO, 
    STDOUT_FILENO,
    STDERR_FILENO,
};

void *sbrk(int increment);
int open(const char *filename, int flag, int mode);
int close(unsigned int fd);
int read(int fd, void *buf, unsigned int count);
int write(int fd, void *buf, unsigned int count);
int ftruncate(int fd, off_t len);
pid_t fork(void);
int isatty(int fd);
int tcsetpgrp(int fildes, pid_t pgid_id);
pid_t tcgetpgrp(int fildes);
pid_t setsid();
pid_t getpid(void);

#endif

