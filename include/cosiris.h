#ifndef _COSIRIS_H
#define _COSIRIS_H

enum {
	SYS_LSTREE,	// 0
	// process
	SYS_FORK,	// 1
	SYS_WAIT,	// 2
	SYS_EXIT,	// 3
	SYS_GETPID,	// 4
	SYS_EXEC,	// 5
	SYS_PS,		// 6
	// mem //
	SYS_SBRK,	// 7
	// files //
	SYS_OPEN,	// 8
	SYS_CLOSE,	// 9
	SYS_STAT,	// 10
	SYS_FSTAT,	// 11
	SYS_READ,	// 12
	SYS_WRITE,	// 13
	SYS_CHDIR,	// 14
	SYS_CHROOT,	// 15
	SYS_CHMOD,	// 16
	SYS_CHOWN,	// 17
	SYS_MKDIR,	// 19
	SYS_COFS_DUMP_CACHE, // 20
	SYS_ISATTY,
	SYS_LSEEK,
};

#endif
