#include <types.h>
#include "syscalls.h"

#define N 1000
int main()
{
	print("fork() stress test\n");
	pid_t pid; int i;
	for(i = 0; i < N; i++) {
		if(!(i % 10)) print(".");
		pid = fork();
		if(pid < 0) {
			print("fork returned -1\n");
			break;
		} else {
			exit(0);
		}
	}
	for(; i > 0; i--) {
		if(!(i % 10)) print("-");
		if((pid = wait(NULL)) < 0) {
			print("Wait exit early\n");
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
