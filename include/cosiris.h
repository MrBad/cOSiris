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
	SYS_ISATTY, // 21
	SYS_LSEEK,	// 22
	SYS_OPENDIR,	// 23
	SYS_CLOSEDIR,	// 24
	SYS_READDIR,	// 25
	SYS_LSTAT,		// 26
	SYS_READLINK,	// 27
	SYS_GETCWD,		// 28
	SYS_UNLINK,		// 29
	SYS_DUP,		// 30
	SYS_PIPE,		// 31
	SYS_CLRSCR,		// 32
	SYS_LINK,		// 33
	SYS_RENAME		// 34
};

#endif
