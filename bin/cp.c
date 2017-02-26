#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "syscalls.h"

#define FS_DIRECTORY    0040000
#define FS_FILE         0100000

//
//	Simple cp program intended for my hobby OS - cOSiris
//		supports: 
//		cp file1 file2 file.. destdir
//		cp file1 destfile
int main(int argc, char *argv[]) 
{
	struct stat st;
	int dst_exist = 0;
	int dst_isdir = 0;
	int i, n, err, fdin, fdout;
	char buf[512];

	if(argc < 3) {
		printf("cp: file1 file2 file.. dest dir or\n");
		printf("cp: file1 file2\n");
		return 1;
	}
	if(stat(argv[argc-1], &st) == 0) {
		dst_exist = 1;
		if(st.st_mode & FS_DIRECTORY)
			dst_isdir = 1;
	}

	if(argc > 3 && (dst_exist == 0 || !(st.st_mode & FS_DIRECTORY))) {
		printf("cp: for multiple files, dest %s should be a directory\n", argv[argc-1]);
		return 1;
	}
	err = 0;
	for(i = 1; i < argc-1; i++) {
		if((fdin = open(argv[i], O_RDONLY, 0)) < 0) {
			printf("cp: cannot open %s\n", argv[i]);
			err++;
			continue;
		}
		if(fstat(fdin, &st) < 0) {
			printf("cp: cannot stat %s\n", argv[i]);
			close(fdin);
			err++;
			continue;
		}
		if(st.st_mode & FS_DIRECTORY) {
			printf("cp: directory copy not supported: %s\n", argv[i]);
			close(fdin);
			err++;
			continue;
		}
		strncpy(buf, argv[argc-1], sizeof(buf)-1);
		if(dst_isdir) {
			strncat(buf, "/", sizeof(buf)-strlen(buf)-1);
			strncat(buf, argv[i], sizeof(buf-strlen(buf)-1));
		}
		if((fdout = open(buf, O_WRONLY|O_CREAT|O_TRUNC, st.st_mode)) < 0){ // change perm //
			printf("cp: cannot open %s\n", argv[argc-1]);
			close(fdin);
			err++;
			continue;
		}
		while((n = read(fdin, buf, sizeof(buf))) > 0) {
			if(write(fdout, buf, n) != n) {
				printf("cp: error in copying %s\n", argv[i]);
				err++;
				break;
			}
		}
		close(fdin);
		close(fdout);
	}
	return err;
}
