#include <cosiris.h>
#include <types.h>
#include "sys.h"

void print(char *str)
{
	_syscall1(SYS_PRINT, (uint32_t)str);
}

void print_int(int n)
{
	_syscall1(SYS_PRINT_INT, (uint32_t) n);
}

// processes
pid_t fork()
{
	return _syscall0(SYS_FORK);
}

pid_t wait(int *status)
{
	return _syscall1(SYS_WAIT, (uint32_t)status);
}

void exit(int status)
{
	_syscall1(SYS_EXIT, (uint32_t) status);
}

pid_t getpid()
{
	return (pid_t) _syscall0(SYS_GETPID);
}

void ps()
{
	_syscall0(SYS_PS);
}

// memory
void * sbrk(int increment)
{
	return (void *) _syscall1(SYS_SBRK, increment);
}


// files
// pipe
//
// int open(char *filename, int flags, ...) {}
// //mkdir
// //mknod
// //chdir
