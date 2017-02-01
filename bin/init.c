#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

extern bool test_mem_1();
int main()
{
	void *start_addr = sbrk(0);
	printf("Testing this %p\n", sbrk(0));
	char *str = malloc(100);
	strcpy(str, "Testing malloc");
	printf("%s\n", str);
	free(str);
	if(test_mem_1()) {
		printf("Testing mem1 OK\n");
	}
	printf("mem used: %d, brk: %p\n", (unsigned int)sbrk(0)-(unsigned int)start_addr, sbrk(0));
	if(test_mem_1()) {
		printf("Testing mem1 again OK\n");
	}
	printf("mem used: %d, brk: %p\n", (unsigned int)sbrk(0)-(unsigned int)start_addr, sbrk(0));

	return 0;
}
