#include <stdio.h>
#include <sys/stat.h>
#include "syscalls.h"

int main(int argc, char *argv[])
{
	struct stat st;
	if(argc != 3) {
		printf("ln: source dest\n");
		return 1;
	}
	if(stat(argv[1], &st) < 0) {
		printf("ln: stat error %s\n", argv[1]);
		return 1;
	}
	if(link(argv[1], argv[2]) < 0) {
		printf("ln: %s %s failed\n", argv[1], argv[2]);
	}
}
