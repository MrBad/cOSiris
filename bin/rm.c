#include <stdio.h>
#include "syscalls.h"


int main(int argc, char *argv[])
{
	if(argc<2) {
		printf("Usage: %s: file\n", argv[0]);
		return 1;
	}

	if(unlink(argv[1]) < 0) {
		printf("Cannot remove %s\n", argv[1]);
		return 1;
	}

	return 0;
}
