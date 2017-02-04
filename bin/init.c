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
		char buf[256];
		int fd;
		fd = open("/dev/console", O_RDWR, 0);
		if(fd < 0) {
			printf("cannot open console\n");
			return 1;
		}
		while(1) {
			printf("# ");
			int numbytes = read(fd, buf, 255);
			buf[numbytes--] = 0;
			while(buf[numbytes] == '\n' || buf[numbytes]==' ' || buf[numbytes]=='\r') {
				buf[numbytes--] = 0;
			}
			if(!strcmp(buf, "ps")) {
				ps();
			} else if (!strcmp(buf, "ls")) {
				// lstree(NULL);
			} else if (!strcmp(buf, "help")) {
				printf("ps - show process list\n");
				// printf("ls - show file tree\n");
				printf("ESC - shut down\n");
			} else {
				printf("%s: command not found - try help\n", buf);
			}
		}
	}
}
