#ifndef _SYSCALLS_H
#define _SYSCALLS_H

#include <sys/stat.h>
#include <dirent.h>

void print(char *str);
// void print_int(int n);
pid_t fork();
pid_t wait(int *status);
void exit(int status);
pid_t getpid();
int exec(char *path, char **argv);
void ps();
void lstree();

void * sbrk(int increment);

// files //
int open(char *filename, int flag, int mode);
int close(unsigned int fd);
int stat(const char *pathname, struct stat *buf);
int fstat(int fd, struct stat *buf);
int read(int fd, void *buf, unsigned int count);
int write(int fd, void *buf, unsigned int count);
int chdir(char *filename);
int chroot(char *filename);
int chown(char *filename, int uid, int gid);
int chmod(const char *filename, mode_t mode);
int mkdir(const char *pathname, mode_t mode);
int isatty(int fd);
off_t lseek(int fd, off_t offset, int whence);


DIR *sys_opendir(char *dirname);
int sys_closedir(DIR *dir);
struct dirent *sys_readdir(DIR *dir);

void cofs_dump_cache();

#endif
