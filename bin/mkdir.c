#include <stdio.h>
#include "syscalls.h"

int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("Usage: mkdir files\n");
		return 1;
	}

	int i;
	for(i = 1; i < argc; i++) {
		if(mkdir(argv[i], 0755) < 0) {
			printf("mkdir: cannot create %s\n", argv[i]);
			return 1;
		}
	}
	return 0;
}
