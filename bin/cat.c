#include <stdio.h>
#include "syscalls.h"


char buf[512];
void cat(int fd)
{
	int i;
	while((i = read(fd, buf, sizeof(buf))) > 0) {
		if(write(1, buf, i) != i) {
			printf("cat: write error\n");
			exit(1);
		}
	}

	if(i < 0) {
		printf("cat: read error\n");
		exit(1);
	}
	if (i == 0)
	    exit(0);
}

int main(int argc, char *argv[])
{
	int i, fd;
	if(argc < 2) {
		cat(0);
		return 0;
	}

	for(i = 1; i < argc; i++) {
		if((fd = open(argv[i], 0, 0)) < 0) {
			printf("cat: cannot open %s\n", argv[i]);
			return 1;
		}
		cat(fd);
		close(fd);
	}
	return 0;
}
