#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "syscalls.h"

#define FS_DIRECTORY    0040000

int main(int argc, char *argv[])
{
	struct stat st;
	int dst_exists = 0, 
		dst_isdir  = 0;
	int i, err;
	char buf[512];

	if(argc < 3) {
		printf("mv: file1 file.. dest dir\n");
		printf("mv: file1 file2\n");
		return 1;
	}
	if(stat(argv[argc-1], &st) == 0) {
		dst_exists = 1;
		if(st.st_mode & FS_DIRECTORY)
			dst_isdir = 1;
	}
	if(argc > 3 && (!dst_exists || !dst_isdir)) {
		printf("mv: for multiple files, dest %s should be a directory\n", argv[argc-1]);
		return 1;
	}
	
	for(i = 1; i < argc-1; i++) {
		if(stat(argv[i], &st) < 0) {
			printf("mv: cannot stat %s\n", argv[i]);
			err++;
			continue;
		}
		strncpy(buf, argv[argc-1], sizeof(buf)-1);
		if(dst_isdir) {
			strncat(buf, "/", sizeof(buf)-strlen(buf)-1);
			strncat(buf, argv[i], sizeof(buf)-strlen(buf)-1);
		}
		if(rename(argv[i], buf) < 0) 
			err++;
	}
	return err;
}
