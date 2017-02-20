#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "syscalls.h"
int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("Usage %s: file\n", argv[0]);
		return 1;
	}

	int fd, len;
	char buf[512];
	strcpy(buf, "testing write file system and creation\n");
	len = strlen(buf);
	fd = open(argv[1], O_RDWR | O_CREAT, 0755);
	if(fd < 0) {
		printf("cannot open file %s\n", argv[1]);
		return 1;
	}
	if(write(fd, buf, len) != len) {
		printf("cannot write to file\n");
		return 1;
	}
	printf("ok\n");
	printf("O_RDWR: %x, O_CREAT: %x, O_TRUNC: %x, O_APPEND: %x\n", O_RDWR, O_CREAT, O_TRUNC, O_APPEND);
	close(fd);

	fd = open(argv[1], O_RDWR | O_TRUNC, 0);
	if(fd < 0) {
		printf("Cannot open file in truncate mode\n");
		return 1;
	}
	if(write(fd, "Testing", 8)!=8) {
		printf("cannot write");
		return 1;
	} 
	close(fd);
}
