#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
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
	struct stat st;
	if(stat(file, &st) < 0) {
		return false;
	}
	return true;
}

int main()
{
	char buf[256];
	while(1) {
		printf("# ");
		int n = read(0, buf, sizeof(buf)-1);
		buf[n--] = 0;
		trim(buf, n);
		if(!strlen(buf)) {
			continue;
		}
		else if(!strcmp(buf, "ps")) {
			ps();
		}
		else if (!strcmp(buf, "lstree")) {
			lstree();
		}
		else if(!strcmp(buf, "cdc")) {
			cofs_dump_cache();
		}
		else if (!strcmp(buf, "help")) {
			printf("ps - show process list\n");
			printf("ls - show files\n");
			printf("lstree - show files tree\n");
			printf("cdc - show cofs dump cache\n");
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
