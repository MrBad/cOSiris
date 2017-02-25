#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"
// dup() test
int main()
{
	int fd;
	char buf[512];
	
	fd = open("test", O_RDWR|O_CREAT, 0666);
	if(!fd) {
		printf("cannot open test file\n");
	}

	close(1); // close stdout

	if(dup(fd) != 1) {
		printf("cannot open test file %d new node\n", fd);
		return 1;
	}
	printf("before writing\n");	
	strcpy(buf, "Testing, this should go to file test");	
	printf("%s\n", buf);
	printf("after writing\n");
	close(fd);
	fd = open("/dev/console", O_RDONLY, 0);
	close(1);
	dup(fd);
	printf("back to normal\n");
	return 0;
}
