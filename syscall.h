#ifndef _SYSCALL_H
#define _SYSCALL_H

/**
 * Initialize system calls
 */
void syscall_init();

/**
 * task_exit alias
 */
void exit(int status);

#endif
