#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"

//
// Testing pipes
//

int main()
{
	pid_t pid;
	int fd[2], n;
	char buf1[512], buf2[512];

	pipe(fd);
	pid = fork();
	if(pid < 0) {
		return 1;
	} else if(pid == 0) {
		close(fd[1]);
		while((n = read(fd[0], buf2, sizeof(buf2))) > 0) {
			buf2[n] = 0;
			printf("read %d bytes from pipe:\n\t%s", n, buf2);
		}
		close(fd[0]);
	} 
	else {
		close(fd[0]);
		strcpy(buf1, "This is a message from parent to child\n");
		int f = open("README", O_RDONLY, 0);
		while((n = read(f, buf1, sizeof(buf1))) > 0) {
			write(fd[1], buf1, n);
		}
		close(fd[1]);
		close(f);
		wait(NULL);
	}
}
