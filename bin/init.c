#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "syscalls.h"

int main()
{
	int fd = open("/README",0,0);
	printf("%d\n", fd);
	char buf[256];
	if(fd < 0) {
		printf("Cannot open file\n");
		return 1;
	}

	pid_t pid = fork();
	if(pid == 0) {
		printf("In child\n");
		while(1) {
			int bytes = read(fd, buf, 255);
			if(bytes < 1) break;
			buf[bytes] = 0;
			printf("%s", buf);
		}
		// close(fd); // should be closed by exit()
		return 0;
	}

	int status;
	pid = wait(&status);
	close(fd);
	printf("Child %d exited with status %d\n", pid, status);
}
