#include <stdio.h>
#include "syscalls.h"


int main()
{
	char buf[512];
	getcwd(buf, 512);
	printf("%s\n", buf);
}
