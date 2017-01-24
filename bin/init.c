#include "syscalls.h"
int main()
{
	char buf[] = "We are in userland\n";
	print(buf);
	char buf2[] = "Horray all, now let's see some process list\n";
	print2(buf2);
	print_int(sum(10, 20));
	print("Let's fork(), baby!\n");
	int pid = fork();
	if(pid == 0) {
		print("I am the child, in user mode, with pid: ");
		print_int(getpid());
		print("\n");
		return 0;
	} else {
		print("I am the parent, in user mode, with pid: ");
		print_int(getpid());
		print("\n");
	}
	ps();
}
