/**
 * Virtual file system
 */
#ifndef _VFS_H
#define _VFS_H

#define FS_DIRECTORY    0040000
#define FS_CHARDEVICE   0020000
#define FS_BLOCKDEVICE  0060000
#define FS_PIPE         0010000
#define FS_FILE         0100000
#define FS_SYMLINK      0120000
#define FS_MOUNTPOINT   0200000 // Is the file an active mountpoint?

#define FS_IFMT         00170000

typedef struct fs_node fs_node_t;

/**
 * Definition of inode operation. 
 * All file system should implement these or part of these functions
 * In the future we will add only a pointer
 *  to a file type operations, to save memory
 */

typedef unsigned int (*read_type_t)(fs_node_t *node, unsigned int offset,
                                    unsigned int size, char *buffer);
typedef unsigned int (*write_type_t)(fs_node_t *node, unsigned int offset,
                                     unsigned int size, char *buffer);
typedef void (*open_type_t)(fs_node_t *node, unsigned int flags);
typedef void (*close_type_t)(fs_node_t *node);
typedef struct dirent *(*readdir_type_t)(fs_node_t *node, unsigned int index);
typedef fs_node_t * (*finddir_type_t)(fs_node_t *node, char *name);
typedef fs_node_t * (*mkdir_type_t)(fs_node_t *node, char *name,
        unsigned int mode);
typedef fs_node_t * (*creat_type_t)(fs_node_t *node, char *name, 
        unsigned int mode);
typedef int(*truncate_type_t)(fs_node_t *node, unsigned int length);
typedef int(*unlink_type_t)(fs_node_t *parent, char *name);
typedef int(*link_type_t)(fs_node_t *parent, fs_node_t *node, char *name);

/**
 * An in-memory representation of an inode
 */
struct fs_node {
    char name[256];             // this info should go ouside inode in future
    unsigned int mask;          //
    unsigned int uid;           //
    unsigned int gid;           //
    unsigned short int type;    // type of file - FS_
    unsigned short int flags;   // various flags -> see pipe
    unsigned int inode;         // inode, unix
    unsigned int size;
    int ref_count;              // number of threads who opened this file
    unsigned int num_links;     // number of files linking to this (on disk links or directory)
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
    link_type_t link;
    void *ptr;                // reserved
};

fs_node_t *fs_root;

/**
 * Opens an inode, with flags
 */
void fs_open(fs_node_t *node, unsigned int flags);

/**
 * Closes an inode
 */
void fs_close(fs_node_t *node);

/**
 * Reads size bytes from inode offset, in buffer.
 */
unsigned int fs_read(fs_node_t *node, unsigned int offset, unsigned int size,
                     char *buffer);
/**
 * Writes size bytes from buffer to inode offset.
 */
unsigned int fs_write(fs_node_t *node, unsigned int offset, unsigned int size,
                      char *buffer);

/**
 * Giving an name, search for it in parent inode node
 */
fs_node_t *fs_finddir(fs_node_t *node, char *name);

/**
 * Reads entry index from inode directory node
 */
struct dirent *fs_readdir(fs_node_t *node, unsigned int index);

/**
 * Creates a directory in parent directory node, with name and mode
 */
fs_node_t *fs_mkdir(fs_node_t *node, char *name, int mode);

/**
 * Duplicates an inode
 */
fs_node_t *fs_dup(fs_node_t *node);

/**
 * Name to inode - gets the inode having path
 */
fs_node_t *fs_namei(char *path);

/**
 * Opens or create/truncate a path and save it in node
 * path - path of the file to open
 * flag - open flags - O_RDONLY, O_CREAT
 * mode - permissions - like 0755
 * TODO: clean it up.
 */
int fs_open_namei(char *path, int flag, int mode, fs_node_t **node);

/**
 * Not yet available
 * int fs_mount(char *path, fs_node_t *node);
 */

/**
 * For debugging purpose - recursive list from parent node
 */
void lstree(fs_node_t *parent, int level);

/**
 * Unlink / removes the path
 */
int fs_unlink(char *path);

/**
 * Rename a file name - needs rewrite
 */
int fs_rename(char *oldpath, char *newpath);

/**
 * Hard link a node to it's parent, with name 
 */
int fs_link(fs_node_t *parent, fs_node_t *node, char *basename);

#endif
