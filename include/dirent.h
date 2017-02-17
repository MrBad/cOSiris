#ifndef _DIRENT_H
#define _DIRENT_H

struct dirent {
	unsigned int d_ino;
	char d_name[28]; // for now
};

typedef struct file DIR;


DIR *opendir(const char *dirname);
int closedir(DIR *);
struct dirent *readdir(DIR *);

#endif
