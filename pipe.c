#include <string.h>
#include "vfs.h"
#include "kheap.h"
#include "task.h"
#include "console.h"

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


//////////////////////////////////////////////////
// TODO - destroy pipe, spinlock
// Need a clean way to schedule next, wait queue,
// put process to sleep -> some clean interface ...
//////////////////////////////////////////////////

void pipe_open(fs_node_t *node, unsigned int flags) {
	vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
	if(flags == PIPE_READ) {
		pipe->readers++;
	}
	if(flags == PIPE_WRITE) {
		pipe->writers++;
	}
}

void pipe_close(fs_node_t *node)
{
	vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
	if(node->flags & PIPE_READ) {
		pipe->readers--;
	}
	if(node->flags & PIPE_WRITE) {
		pipe->writers--;
	}
	if(!pipe->readers && !pipe->writers)
	{
		// close and destroy pipe //
		kprintf("TODO: destroy pipe\n");
	}
}

unsigned int pipe_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
	unsigned int bytes = 0;
	while(bytes == 0) {
		if(!pipe->writers) {
			buffer[bytes++] = 0;
			return bytes;
		}
		while(pipe->write_pos > pipe->read_pos && bytes < size) {
			buffer[bytes] = pipe->buffer[pipe->read_pos % pipe->size];
			bytes++; pipe->read_pos++;
		}

		wait_queue_t *q;
		for(q = pipe->wait_queue; q; q = q->next) {
			task_t *t = get_task_by_pid(q->pid);
			t->state = TASK_READY;
		}
		if(bytes == 0) {
			// add me to the waiting q //
			wait_queue_t *q, *n;
			n = (wait_queue_t *) calloc(1, sizeof(wait_queue_t));
			n->pid = current_task->pid;
			q = pipe->wait_queue;
			if(!q) {
				pipe->wait_queue = n;
			} else {
				while(q->next) q = q->next;
				q->next = n;
			}
			// put me to sleep and switch task //
			current_task->state = TASK_SLEEPING;
			task_switch();
		}
	}
	return bytes;
}

unsigned int pipe_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
	unsigned int bytes = 0;
	while(bytes < size) {
		if(!pipe->readers) {
			while (bytes < size) {
				pipe->buffer[pipe->write_pos % pipe->size] = buffer[bytes];
				bytes++; pipe->write_pos++;
			}
		} else {
			while((pipe->write_pos - pipe->read_pos) < pipe->size && bytes < size) {
				pipe->buffer[pipe->write_pos % pipe->size] = ((char *)buffer)[bytes];
        		bytes++;
        		pipe->write_pos++;
      		}
		}

		wait_queue_t *q;
		for(q = pipe->wait_queue; q; q = q->next) {
			task_t *t = get_task_by_pid(q->pid);
			t->state = TASK_READY;
		}

		if(bytes < size) {
			// add me to the waiting q //
			wait_queue_t *q, *n;
			n = (wait_queue_t *) calloc(1, sizeof(wait_queue_t));
			n->pid = current_task->pid;
			q = pipe->wait_queue;
			if(!q) {
				pipe->wait_queue = n;
			} else {
				while(q->next) q = q->next;
				q->next = n;
			}
			// put me to sleep and switch task //
			current_task->state = TASK_SLEEPING;
			task_switch();
		}
	}
	return bytes;
}


int new_pipe(fs_node_t **nodes)
{
	vfs_pipe_t *pipe = (vfs_pipe_t *) calloc(1, sizeof(vfs_pipe_t));
	pipe->buffer = (char *) malloc(PIPE_BUFFER_SIZE);
	pipe->size = PIPE_BUFFER_SIZE;

	nodes[0] = (fs_node_t *)calloc(1, sizeof(fs_node_t));
	strcpy(nodes[0]->name, "[pipe:read]");
	nodes[0]->flags = FS_PIPE | PIPE_READ;
	nodes[0]->open = pipe_open;
	nodes[0]->close = pipe_close;
	nodes[0]->read = pipe_read;
	nodes[0]->write = 0;
	nodes[0]->ptr = pipe;

	nodes[1] = (fs_node_t *) calloc(1, sizeof(fs_node_t));
	strcpy(nodes[1]->name, "[pipe:write]");
	nodes[1]->flags = FS_PIPE | PIPE_WRITE;
	nodes[1]->open = pipe_open;
	nodes[1]->close = pipe_close;
	nodes[1]->read = 0;
	nodes[1]->write = pipe_write;
	nodes[1]->ptr = pipe;

	return 0;
}
