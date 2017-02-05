#ifndef _SYSFILE_H
#define _SYSFILE_H

#include <sys/stat.h>
#include "vfs.h"

#define PROC_MAX_OPEN_FILES 256 // maximum number of open files //

struct file {
	unsigned short mode;
	unsigned short flags;
	unsigned int offs;
	fs_node_t *fs_node;
};

int sys_exec(char *path, char *argv[]);
int sys_open(char *path, int flag, int mode);
int sys_close(unsigned int fd);
int sys_stat(char *path, struct stat *buf);
int sys_fstat(int fd, struct stat *buf);
int sys_read(int fd, void *buf, size_t count);
int sys_write(int fd, void *buf, size_t count);
int sys_chdir(char *path);
int sys_chroot(char *path);
int sys_chmod(char *filename, int uid, int gid);
int sys_chown(char *filename, int mode);
int sys_mkdir(char *pathname, int mode);

#endif
