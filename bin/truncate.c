#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "syscalls.h"

int main()
{
	int fd = open("kernel.lst", O_RDWR|O_TRUNC, 0);
	if(!fd) {
		printf("open\n");
		return 1;
	}
	struct stat st;
	if(fstat(fd, &st) < 0) {
		printf("fstat\n");
		return 1;
	}	
	printf("size: %d\n", st.st_size);

	close(fd);
}
