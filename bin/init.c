#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"

int main()
{
	printf("In init\n");
	pid_t pid = fork();
	if(pid == 0) {
		char *margv[] = {"cO sh", "this", 0};
		if(exec("/cosh", margv) < 0) {
			printf("Cannot load cosh\n");
			return 1;
		}
	}
	return 0;
}
