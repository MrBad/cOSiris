#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"
int main()
{
	char buf[512];
	int fd;
	if((fd = open("ta", O_CREAT|O_WRONLY, 644)) < 0) {
		printf("cannot open\n");
		return 1;
	}
	strcpy(buf, "Creating the file\n");
	if(write(fd, buf, strlen(buf)) <= 0){
		printf("write");
		return 1;
	}
	close(fd);
	
	
	if((fd = open("ta", O_RDWR|O_APPEND, 0)) < 0) {
		printf("cannot open file\n");
		return 1;
	}
	
	strcpy(buf, "Second line of file\n");
	if(write(fd, buf, strlen(buf)) <= 0) {
		printf("write 2");
		return 1;
	}

	close (fd);


}
