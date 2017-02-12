#ifndef _DIRENT_H
#define _DIRENT_H

struct dirent {
	unsigned int d_ino;
	char d_name[28]; // for now
};

#endif
