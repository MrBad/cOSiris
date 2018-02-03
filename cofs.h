/**
 * COFS file system implementation.
 * Inspired from Unix V6 and xv6 reimplementation.
 */
#ifndef _COFS_H
#define _COFS_H

#include "vfs.h"

#define COFS_MAGIC 0xC0517155    // cOSiris magic number

typedef struct cofs_superblock {
    unsigned int magic;
    unsigned int size;              // total size of fs, in blocks
    unsigned int num_blocks;        // number of data blocks
    unsigned int num_inodes;        // number of inodes
    unsigned int bitmap_start;      // where free bitmap starts
    unsigned int inode_start;       // where inodes starts
    unsigned int data_block;        // where free blocks starts
} cofs_superblock_t;

#define NUM_DIRECT  6                           // number of direct blocks
#define NUM_SIND    (BLOCK_SIZE/sizeof(int))    // number of single indirect blocks
#define NUM_DIND    (NUM_SIND * NUM_SIND)       // number of double indirect blocks
// maximum file size in blocks ~ 8 MB on 512 BLOCK_SIZE
#define MAX_FILE_SIZE NUM_DIRECT + NUM_SIND + NUM_DIND

#define SIND_IDX NUM_DIRECT                    // single indirect index int inode addrs
#define DIND_IDX NUM_DIRECT + 1                // double indirect index into inode addrs

// number of inodes per block //
#define NUM_INOPB (BLOCK_SIZE/sizeof(cofs_inode_t))
// block witch contains inode n
#define INO_BLK(ino, superblock)    ((ino)/NUM_INOPB + superblock.inode_start)
#define BITS_PER_BLOCK (BLOCK_SIZE * 8)
// block of bitmap containing bit for block b //
#define BITMAP_BLOCK(block, superblock) (block/BITS_PER_BLOCK + superblock.bitmap_start)

/**
 * Cofs inode
 * Must be a sub multiple of BLOCK_SIZE.
 */
typedef struct cofs_inode {
    unsigned short int type; // file type + access mode
    unsigned short int major;
    unsigned short int minor;
    unsigned short int uid;
    unsigned short int gid;
    unsigned short int num_links;
    unsigned int atime, mtime, ctime;
    unsigned int size;
    unsigned int addrs[NUM_DIRECT + 3]; // + SNG_IDX + DBL_IDX + reserved for triple idx
} cofs_inode_t;

// Must be a sub multiple of BLOCK_SIZE
#define DIRSIZE 28 // filename length limit
struct cofs_dirent {
    unsigned int inode;
    char name[DIRSIZE];
};
typedef struct cofs_dirent cofs_dirent_t;

/**
 * Initialize the file system
 */
fs_node_t *cofs_init();

/**
 * Dumps cofs inode cache
 */
void cofs_dump_cache();

#endif
