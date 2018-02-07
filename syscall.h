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

/**
 * Validates a pointer
 */
void validate_usr_ptr(void *ptr);

#endif
