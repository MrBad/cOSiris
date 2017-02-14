#include <stdio.h>
#include <sys/types.h>
#include "syscalls.h"

#define PAGE_SIZE 0x1000
int main()
{
	printf("testing sbrk\n");
	void * addr, * addr_new;
	addr = sbrk(0);
	addr_new = sbrk(PAGE_SIZE);
	if(addr != addr_new) {
		printf("sbrk failed on increment\n");
		return 1;
	}
	addr_new = sbrk(0);
	if((unsigned int)addr + PAGE_SIZE != (unsigned int)addr_new) {
		printf("sbrk failed on increment a PAGE_SIZE\n");
		return 1;
	}
	sbrk(-PAGE_SIZE);
	addr_new = sbrk(0);
	if(addr != addr_new) {
		printf("sbrk failed on decrement a PAGE_SIZE\n");
		return 1;
	}
	printf("Test passed OK\n");
	exit(0);
}
