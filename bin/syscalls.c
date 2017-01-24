#include <cosiris.h>
#include "sys.h"

void print(char *str)
{
	_syscall1(SYS_PRINT, (uint32_t)str);
}

void print2(char *str)
{
	_syscall1(SYS_PRINT2, (uint32_t) str);
}

void ps()
{
	_syscall0(SYS_PS);
}

void exit(uint32_t status)
{
	_syscall1(SYS_EXIT, status);
}

uint32_t sum(uint32_t a, uint32_t b)
{
	return _syscall2(SYS_SUM, a, b);
}

void print_int(uint32_t n)
{
	_syscall1(SYS_PRINT_INT, n);
}

int fork()
{
	return _syscall0(SYS_FORK);
}

int getpid()
{
	return _syscall0(SYS_GETPID);
}
