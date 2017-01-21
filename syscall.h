#ifndef _SYSCALL_H
#define _SYSCALL_H


void switch_to_user_mode();
void syscall_init();

void syscall_print(char *buf);
void syscall_print2(char *buf);
#endif
