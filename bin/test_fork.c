#include <types.h>
#include "syscalls.h"

#define N 50
int main()
{
	pid_t pid; int i;
	for(i = 0; i < N; i++) {
		pid = fork();
		if(pid < 0) {
			break;
		} else {
			exit(0);
		}
	}
	for(; i > 0; i--) {
		if(wait(NULL) < 0) {
			print("Wait exit early\n");
			exit(0);
		}
	}
	if(wait(NULL) != -1) {
		print("Wait got too many\n");
		exit(0);
	}

	print("Fork test OK\n");
	ps();
}
