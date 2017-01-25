#ifndef _SYSCALLS_H
#define _SYSCALLS_H

void print(char *str);
void print_int(int n);
pid_t fork();
pid_t wait(int *status);
void exit(int status);
pid_t getpid();
void ps();

#endif
