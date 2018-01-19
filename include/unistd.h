#ifndef _UNISTD_H
#define _UNISTD_H

void *sbrk(int increment);
int write(int fd, void *buf, unsigned int count);
#endif
