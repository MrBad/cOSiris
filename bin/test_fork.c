#include <sys/types.h>
#include "syscalls.h"
#include <stdio.h>

#define N 1000
int main()
{
	pid_t pid, wp; int status;
	printf("\nWe are in userland\n");
	printf("Let's fork(), baby!\n");
	pid = fork();
	if(pid == 0) {
		printf("I am the child\n");
		return 2;
	} else {
		wp = wait(&status);
		if(pid != wp) {
			printf("fork returned different pid\n");
			exit(1);
		}
		if(status != 2) {
			printf("fork returned different exit status\n");
			exit(2);
		}
		printf("OK child %d exited with status: %d\n", wp, status);
	}
	status = -1;
	wp = 0;
	wp = wait(&status);
	if(wp != -1) {
		printf("wait too many\n");
		exit(4);
	}
	printf("OK child %d exited with status: %d\n", wp, status);
	ps();

	printf("fork() stress test\n");
	int i;
	for(i = 0; i < N; i++) {
		if(!(i % 10)) printf(".");
		pid = fork();
		if(pid < 0) {
			printf("fork returned -1\n");
			break;
		} else {
			exit(0);
		}
	}
	for(; i > 0; i--) {
		if(!(i % 10)) print("-");
		if((pid = wait(NULL)) < 0) {
			printf("Wait exit early\n");
			exit(0);
		}
		// print_int(pid); print(" exited\n");
	}

	if(wait(NULL) != -1) {
		print("Wait got too many\n");
		exit(0);
	}

	print("Fork test OK\n");
	ps();
}
