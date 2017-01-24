#include "syscalls.h"
int main()
{
	char buf[] = "We are in userland\n";
	print(buf);
	char buf2[] = "Horray all, now let's see some process list\n";
	print2(buf2);
	print_int(sum(10, 20));
	// fork();
	ps();
}
