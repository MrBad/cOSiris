#include <types.h>
#include "syscalls.h"

int main()
{
	print("\nWe are in userland\n");
	print("Let's fork(), baby!\n");
	pid_t pid = fork();
	if(pid == 0) {
		print("I am the child\n");
		return 1;
	} else {
		print("I am the parent\n");
	}

	pid_t wp=0; int status;
	wp = wait(& status);
	print("Child ");print_int(wp); print(" exited with status: ");print_int(status);print("\n");
	ps();
	return 0;
}
