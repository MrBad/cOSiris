#include <types.h>
#include "syscalls.h"
int main()
{
	print("\nWe are in userland\n");
	print("Let's fork(), baby!\n");
	pid_t pid = fork();
	if(pid == 0) {
		print("I am the child, in user mode, with pid: "); print_int(getpid()); print("\n");
		pid = fork();
		if(pid == 0) {
			print("I am grand-grand child: ");print_int(getpid()); print("\n");
			return 4;
		}
		int x = wait(NULL);
		print("Child, waked by: "); print_int(x); print("\n");
		return 2;
	} else {
		pid_t i = wait(NULL);
		print("I am the parent, in user mode, with pid: ");print_int(getpid());print(" waked up by ");print_int(i);print("\n");
		pid = fork();
		if(pid == 0) {
			print("I am second child with pid: ");print_int(getpid()); print("\n");
			return 3;
		}
		 else {
			pid_t i2 = wait(NULL);
			print("Parent, 2, waked up by pid: ");print_int(i2);print("\n");
		 }
	}

	pid_t i3 = wait(NULL);
	print("Horray all, now let's see some process list: ");print_int(i3);print("\n");
	ps();
	return 1;
}
