#ifndef _SYSPROC_H
#define _SYSPROC_H

#include "signal.h"

int sys_exec(char *path, char *argv[]);
int sys_kill(int pid, int signum);
sighandler_t sys_signal(int signum, sighandler_t handler);

#endif
