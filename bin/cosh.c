#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "syscalls.h"


// buf to read in the line, max number of chars
// return length of line //
int read_line(char *buf, int max) {
	int n, i;
	n = read(0, buf, max-1);
	if(n == 0) return n;
	buf[n] = 0;
	// clean left //
	for(i = 0; i < n; i++) {
		if(buf[i]!=' '&&buf[i]!='\t') {
			n = n - i;
			strncpy(buf, buf+i, n);
			buf[n]=0;
			break;
		}
	}
	// clean right //
	while((n > 0)&&(buf[n-1]==' '||buf[n-1]=='\t'||buf[n-1]=='\r'||buf[n-1]=='\n'))
		buf[--n]=0;

	return n;
}

// is destroying buf
char *read_token(char *buf, char *prev_token, unsigned int len)
{
	char *p;
	if(prev_token == NULL) { // first time here
		prev_token = buf;
		for(p = buf; p < buf+len; p++) {
			if(*p==' ' || *p=='\t') {
				*p = 0;
			}
		}
		return prev_token;
	}
	int found = 0;
	for(p = prev_token;p < buf + len; p++) {
		if(*p == 0) {
			found = 1;
			continue;
		}
		if(found) {
			prev_token = p;
			return p;
		}
	}
	return NULL;
}

#define MAX_ARGC 10
int main() {
	printf("cOSh - cOSiris shell\n");
	char buf[256];
	char *argv[MAX_ARGC];
	int argc;
	char *token;
	int len;
	pid_t pid;
	while(1) {
		write(1, "# ", 2);
		len = read_line(buf, sizeof(buf));
		if(len <= 0) {	// discard empty line //
			continue;
		}

		token = NULL;
		for(argc = 0; argc < MAX_ARGC; argc++) {
			token = read_token(buf, token, len);
			if(!token) {
				break;
			}
			argv[argc] = token;
		}
		argv[argc] = 0;

		if(!strcmp(argv[0], "help")) {
			printf("Internal commands\n");
			printf("ps - show process list\n");
			printf("lstree - show files tree\n");
			printf("cdc - show cofs dump cache\n");
			printf("ESC - shut down\n");
		}
		else if(!strcmp(argv[0], "ps")) {
			ps();
		}
		else if (!strcmp(argv[0], "lstree")) {
			lstree();
		}
		else if(!strcmp(argv[0], "cdc")) {
			cofs_dump_cache();
		}
		else {
			// try to execute command //
			pid = fork();
			if(pid == 0) {
				exit(exec(argv[0], argv));
			} else if(pid < 0) {
				printf("error forking\n");
			} else {
				int status;
				pid = wait(&status);
				if(status!=0) {
					printf("cosh: error executing %s\n", argv[0]);
				}
			}
		}
	}
	return 0;
}
