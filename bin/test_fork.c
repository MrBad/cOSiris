#include <sys/types.h>
#include "syscalls.h"

#define N 1000
int main()
{
	pid_t pid, wp; int status;
	print("\nWe are in userland\n");
	print("Let's fork(), baby!\n");
	pid = fork();
	if(pid == 0) {
		print("I am the child\n");
		return 2;
	} else {
		wp = wait(&status);
		if(pid != wp) {
			print("fork returned different pid\n");
			exit(1);
		}
		if(status != 2) {
			print("fork returned different exit status\n");
			exit(2);
		}
		print("OK child ");print_int(wp); print(" exited with status: ");print_int(status);print("\n");
	}
	status = -1;
	wp = 0;
	wp = wait(&status);
	if(wp != -1) {
		print("wait too many\n");
		exit(4);
	}
	print("Child ");print_int(wp); print(" exited with status: ");print_int(status);print("\n");
	ps();

	print("fork() stress test\n");
	int i;
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
