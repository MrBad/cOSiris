#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../include/dirent.h"

#define FS_DIRECTORY    0040000
#define FS_CHARDEVICE   0020000
#define FS_BLOCKDEVICE  0060000
#define FS_PIPE         0010000
#define FS_FILE         0100000
#define FS_SYMLINK      0120000
#define FS_MOUNTPOINT   0200000 

int main(int argc, char *argv[])
{
	DIR *d;
	struct dirent *dir;
	char *path;
	char buf[512];
	struct stat st;

	if(argc < 2) {
		path = strdup(".");
	} else {
		path = strdup(argv[1]);
	}
	
	if(stat(path, &st) < 0) {
		printf("ls: cannot stat %s\n", path);
		return 1;
	}

	printf("type: %x\n", st.st_mode);
	if((st.st_mode & FS_FILE)) {
		printf("%s %d %d %d\n", path, st.st_mode, st.st_ino, st.st_size);
	} 
	else if(st.st_mode & FS_DIRECTORY) {
		printf("type ino  size   name\n");
		d = opendir(path);
		if(!d) {
			printf("cannot open %s\n", path);
		}
		while((dir = readdir(d))) {
			if(dir->d_ino == 0)
				continue;
			strncpy(buf, path, sizeof(buf)-1);
			strcat(buf, "/");
			strncat(buf, dir->d_name, sizeof(buf)-1-strlen(buf)-1);
			if(stat(buf, &st) < 0) {
				printf("ls: cannot stat %s\n", buf);
			    continue;
			}
			printf("%-4c %-4d %-6d %s\n", (st.st_mode & FS_DIRECTORY) ? 'd': (st.st_mode & FS_CHARDEVICE) ? 'c':'-', st.st_ino, st.st_size, dir->d_name);
		}
		closedir(d);
		return 0;
	}
}
