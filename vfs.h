//
// Created by root on 27/01/16.
//
#ifndef _VFS_H
#define _VFS_H


#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x04
#define FS_BLOCKDEVICE 0x08
#define FS_PIPE        0x10
#define FS_SYMLINK     0x20
#define FS_MOUNTPOINT  0x40 // Is the file an active mountpoint?


struct fs_node;

typedef unsigned int (*read_type_t)(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer);

typedef unsigned int (*write_type_t)(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer);

typedef void (*open_type_t)(struct fs_node *node, unsigned int flags);

typedef void (*close_type_t)(struct fs_node *node);

typedef struct dirent *(*readdir_type_t)(struct fs_node *node, unsigned int index);

typedef struct fs_node *(*finddir_type_t)(struct fs_node *node, char *name);

typedef struct fs_node {
	char name[128];
	unsigned int mask;
	unsigned int uid;
	unsigned int gid;
	unsigned int flags;
	unsigned int inode;
	unsigned int length;
	unsigned int impl;      // ?! implementation defined number

	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;

	struct fs_node *ptr; // Used in mountpoints and symlinks
} fs_node_t;

struct dirent {
	char name[128];     // Filename
	unsigned int inode;
};


unsigned int read_fs(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);

unsigned int write_fs(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);

void open_fs(fs_node_t *node, unsigned int flags);

void close_fs(fs_node_t *node);

fs_node_t *finddir_fs(fs_node_t *node, char *name);

struct dirent *readdir_fs(fs_node_t *node, unsigned int index);

fs_node_t *finddir_fs(fs_node_t *node, char *name);


#endif
