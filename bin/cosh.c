#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"

//
//	cOSiris primitive shell //
//

void trim(char *str, int len) {
	while(str[len]==' ' || str[len]=='\n' || str[len]=='\r' || str[len]=='\t') {
		str[len--] = 0;
	}
}

bool is_file(char *file) { // todo - change to fstat when we will have it
	int fd = open(file, O_RDONLY, 0);
	if(fd < 0) {
		return false;
	} else {
		close(fd);
		return true;
	}
}

int main(int argc, char *argv[])
{
	// unsigned int *esp = (unsigned int *)(0x0FFFF000);
	// int i;
	// printf("ARGV: %p\n", &argv);
	// i = (unsigned int *)&argv;
	// for(;i < esp; i++) {
		// printf("i:%p, esp: %p, val:%p, %s\n", i, esp, *i, i<esp?"YES":"NO");
	// }{
	// for(i =0; i < argc; i++)
	// 	printf("argc:%d, argv:%s\n", i, argv[i]);

	char buf[256];
	while(1) {
		printf("# ");
		int n = read(0, buf, 255);
		buf[n--] = 0;
		trim(buf, n);
		if(!strlen(buf)) {
			continue;
		}
		else if(!strcmp(buf, "ps")) {
			ps();
		}
		else if (!strcmp(buf, "ls")) {
			lstree();
		}
		else if (!strcmp(buf, "help")) {
			printf("ps - show process list\n");
			printf("ls - show file tree\n");
			printf("ESC - shut down\n");
		}
		else if(is_file(buf)) {
			pid_t pid = fork();
			if(pid == 0) {
				exec(buf, NULL);
				exit(0);
			} else {
				int status;
				pid_t pid = wait(&status);
				printf("%d exited with status %d\n", pid, status);
			}
		}
		else {
			printf("%s: command not found - try help\n", buf);
		}
	}
}
