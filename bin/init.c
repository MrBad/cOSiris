#include <types.h>
#include "syscalls.h"

// int main()
// {
// 	print("\nWe are in userland\n");
// 	print("Let's fork(), baby!\n");
// 	pid_t pid = fork();
// 	if(pid == 0) {
// 		print("I am the child\n");
// 		return 1;
// 	} else {
// 		wait(NULL);
// 		print("I am the parent\n");
// 	}
//
// 	pid_t wp=0; int status;
// 	wp = wait(& status);
// 	print("Child ");print_int(wp); print(" exited with status: ");print_int(status);print("\n");
// 	ps();
// 	return 0;
// }

#define N 1000
int main()
{
	pid_t pid; int i;
	for(i = 0; i < N; i++) {
		pid = fork();
		if(pid < 0) {
			print("fork returned -1\n");
			break;
		} else {
			exit(0);
		}
	}
	for(; i > 0; i--) {
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
