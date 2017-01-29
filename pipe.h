#ifndef _PIPE_H
#define _PIPE_H

#include "vfs.h"
#include "task.h"

#define PIPE_BUFFER_SIZE 512
typedef struct vfs_pipe {
	char *buffer;
	unsigned int size;
	unsigned int flags;
	unsigned int readers;
	unsigned int writers;
	unsigned int read_pos;
	unsigned int write_pos;
	wait_queue_t *wait_queue; // pipe waiting queue
} vfs_pipe_t;

#define PIPE_READ 1
#define PIPE_WRITE 2


int new_pipe(fs_node_t *nodes[2]);

#endif
