#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "syscalls.h"

#define FS_FILE			0x01
#define FS_DIRECTORY	0x02
#define FS_CHARDEVICE 	0x04
#define FS_BLOCKDEVICE	0x08
#define FS_PIPE			0x10
#define FS_SYMLINK		0x20
#define FS_MOUNTPOINT	0x40
#define FS_VALID		0x80


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
		printf("Cannot open %s\n", path);
		return 1;
	}

	if(fstat(fd, &st) < 0) {
		printf("Cannot stat %s\n", path);
		return 1;
	}

	if(st.st_mode & FS_FILE) {
		printf("%s %d %d %d\n", path, st.st_mode, st.st_ino, st.st_size);
	}
	else if(st.st_mode & FS_DIRECTORY) {
		while(read(fd, &dir, sizeof(struct dirent))) {
			if(dir.d_ino == 0) {
				continue;
			}
			strncpy(buf, path, sizeof(buf)-1);
			strcat(buf, "/");
			strncat(buf, dir.d_name, sizeof(buf)-1-strlen(buf)-1);
			if(stat(buf, &st)<0) {
				printf("Cannot stat %s\n", buf);
				continue;
			}
			printf("%c %-3d %-6d %s\n", st.st_mode==FS_DIRECTORY?'d':'-', st.st_ino, st.st_size, dir.d_name);
		}
	}
}
