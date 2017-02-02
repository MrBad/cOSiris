#ifndef _STAT_H
#define _STAT_H

#include <sys/types.h>

struct stat {
	dev_t   st_dev;
	ino_t   st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t   st_uid;
	gid_t   st_gid;
	dev_t   st_rdev;
	off_t   st_size;
	time_t  st_atime;
	time_t  st_mtime;
	time_t  st_ctime;
};


//chmod; fstat; mkdir; mkfifo; stat; umask;
int chmod(char *filename, int uid, int gid);
int fstat(int fd, struct stat *buf);
int mkdir(const char *pathname, int mode);
int mkfifo(const char *pathname, int mode);
int stat(const char *pathname, struct stat *buf);
mode_t umask(mode_t mask);

#endif
