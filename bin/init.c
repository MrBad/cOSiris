#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"

int main()
{
	pid_t pid = fork();
	if(pid == 0) {
		printf("Loading cosh\n");
		if(exec("/cosh") < 0) {
			printf("Cannot load cosh\n");
			return 1;
		}
	}
	return 0;
}
