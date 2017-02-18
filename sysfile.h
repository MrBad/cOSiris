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
typedef struct file DIR;

int sys_exec(char *path, char *argv[]);
int sys_open(char *path, int flag, int mode);
int sys_close(unsigned int fd);
int sys_stat(char *path, struct stat *buf);
int sys_fstat(int fd, struct stat *buf);
int sys_lstat(char *path, struct stat *buf);
int sys_read(int fd, void *buf, size_t count);
int sys_write(int fd, void *buf, size_t count);
int sys_chdir(char *path);
int sys_chroot(char *path);
int sys_chown(char *filename, int uid, int gid);
int sys_chmod(char *filename, int mode);
int sys_mkdir(char *pathname, int mode);
int sys_isatty(int fd);
off_t sys_lseek(int fd, off_t offset, int whence);

// dir //
DIR *sys_opendir(char *dirname);
int sys_closedir(DIR *);
int sys_readdir(DIR *dir, struct dirent *buf);
int lstat(const char *pathname, struct stat *buf);
int sys_readlink(const char *pathname, char *buf, size_t bufsiz);

/*
int readdir_r(DIR *, struct dirent *, struct dirent **);
void rewinddir(DIR *);
void seekdir(DIR *, long int);
long int telldir(DIR *);
*/

char *sys_getcwd(char *buf, size_t size);
#endif
