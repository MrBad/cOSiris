#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/dirent.h"
#include "syscalls.h"

#define FS_DIRECTORY	0040000
#define FS_CHARDEVICE	0020000
#define FS_BLOCKDEVICE	0060000
#define FS_PIPE			0010000
#define FS_FILE			0100000
#define FS_SYMLINK		0120000
#define FS_MOUNTPOINT	0200000 // Is the file an active mountpoint?

int main(int argc, char *argv[])
{
	char path[512];
	char buf[512];
	int fd;
	struct stat st;
	struct dirent dir;
	if(argc < 2) {
		strcpy(path, ".");
	} else {
		strncpy(path, argv[1], sizeof(path)-1);
	}

	if((fd = open(path, O_RDONLY,0)) < 0) {
		printf("ls: cannot open %s\n", path);
		return 1;
	}

	if(fstat(fd, &st) < 0) {
		printf("ls: cannot stat %s\n", path);
		return 1;
	}

	if(st.st_mode & FS_FILE) {
		printf("%s %d %d %d\n", path, st.st_mode, st.st_ino, st.st_size);
	}
	else if(st.st_mode & FS_DIRECTORY) {
		printf("type ino  size   name\n");
		while(read(fd, &dir, sizeof(struct dirent))) {
			if(dir.d_ino == 0) {
				continue;
			}
			strncpy(buf, path, sizeof(buf)-1);
			strcat(buf, "/");
			strncat(buf, dir.d_name, sizeof(buf)-1-strlen(buf)-1);
			if(stat(buf, &st)<0) {
				printf("ls: cannot stat %s\n", buf);
				continue;
			}
			printf("%-4c %-4d %-6d %s\n", (st.st_mode & FS_DIRECTORY) ? 'd': (st.st_mode & FS_CHARDEVICE) ? 'c':'-', st.st_ino, st.st_size, dir.d_name);
		}
	}
	close(fd);
}
