#ifndef _PIPE_H
#define _PIPE_H

#include "i386.h"	// spinlock
#include "vfs.h"
#include "task.h"
#include "list.h"

#define PIPE_BUFFER_SIZE 1024
typedef struct vfs_pipe {
    char *buffer;
    unsigned int size;
    // unsigned int flags;
    unsigned int readers;       // usually max 1 reader and 1 writer.
    unsigned int writers;       // but we must keep track of 0
    unsigned int read_pos;      // read position in pipe buffer
    unsigned int write_pos;     // write position in pipe buffer
    spin_lock_t lock;
} vfs_pipe_t;

#define PIPE_READ 1
#define PIPE_WRITE 2

int pipe_new(fs_node_t *nodes[2]);

#endif
