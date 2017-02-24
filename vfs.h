//
// Created by root on 27/01/16.
//
#ifndef _VFS_H
#define _VFS_H

// #include "x86.h"
//
// #define FS_FILE			0x01
// #define FS_DIRECTORY	0x02
// #define FS_CHARDEVICE 	0x04
// #define FS_BLOCKDEVICE	0x08
// #define FS_PIPE			0x10
// #define FS_SYMLINK		0x20
// #define FS_MOUNTPOINT	0x40 // Is the file an active mountpoint?
// #define FS_VALID		0x80


#define FS_DIRECTORY	0040000
#define FS_CHARDEVICE 	0020000
#define FS_BLOCKDEVICE	0060000
#define FS_PIPE			0010000
#define FS_FILE			0100000
#define FS_SYMLINK		0120000
#define FS_MOUNTPOINT	0200000 // Is the file an active mountpoint?

struct fs_node;

typedef unsigned int (*read_type_t)(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer);
typedef unsigned int (*write_type_t)(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer);
typedef void (*open_type_t)(struct fs_node *node, unsigned int flags);
typedef void (*close_type_t)(struct fs_node *node);
typedef struct dirent *(*readdir_type_t)(struct fs_node *node, unsigned int index);
typedef struct fs_node *(*finddir_type_t)(struct fs_node *node, char *name);
typedef struct fs_node *(*mkdir_type_t)(struct fs_node *node, char *name, unsigned int mode);
typedef struct fs_node *(*creat_type_t)(struct fs_node *node, char *name, unsigned int mode);
typedef int(*truncate_type_t)(struct fs_node *node, unsigned int length);
typedef int(*unlink_type_t)(struct fs_node *parent, char *name);


typedef struct fs_node {
	char name[256];				// this info should go ouside inode in future
	unsigned int mask;			//
	unsigned int uid;			//
	unsigned int gid;			//
	unsigned short int type;	// type of file - FS_
	unsigned short int flags;	// various flags -> see pipe
	unsigned int inode;			// inode, unix
	unsigned int size;
	int ref_count;				// number of threads who open this file
	unsigned int num_links;		// number of files linking to this (on disk links or directory)
	unsigned int atime, ctime, mtime;
	unsigned int *addrs;
	int impl;

	unsigned int lock;
	// operations //
	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;
	mkdir_type_t mkdir;
	creat_type_t creat;
	truncate_type_t truncate;
	unlink_type_t unlink;
	void *ptr;				// reserved
} fs_node_t;

//struct dirent {
//	char name[256];     // Filename
//	unsigned int inode;
//};


fs_node_t *fs_root;


void fs_open(fs_node_t *node, unsigned int flags);
void fs_close(fs_node_t *node);
unsigned int fs_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);
unsigned int fs_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);


fs_node_t *fs_finddir(fs_node_t *node, char *name);
struct dirent *fs_readdir(fs_node_t *node, unsigned int index);
fs_node_t * fs_mkdir(fs_node_t *node, char *name, int mode);
fs_node_t *fs_dup(fs_node_t *node);
fs_node_t *fs_namei(char *path);
int fs_open_namei(char *path, int flag, int mode, fs_node_t **node);
int fs_mount(char *path, fs_node_t *node);
void lstree(fs_node_t *parent, int level);
int fs_unlink(char *path);



#endif
