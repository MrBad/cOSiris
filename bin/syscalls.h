#ifndef _SYSCALLS_H
#define _SYSCALLS_H

#include <sys/stat.h>

void print(char *str);
// void print_int(int n);
pid_t fork();
pid_t wait(int *status);
void exit(int status);
pid_t getpid();
void ps();
void lstree();

void * sbrk(int increment);

// files //
int open(char *filename, int flag, int mode);
int close(unsigned int fd);
int stat(const char *pathname, struct stat *buf);
int fstat(int fd, struct stat *buf);
int read(int fd, void *buf, unsigned int count);
int write(int fd, void *buf, unsigned int count);
int chdir(char *filename);
int chroot(char *filename);
int chmod(char *filename, int uid, int gid);
int chown(char *filename, int mode);
int mkdir(const char *pathname, int mode);

#endif
