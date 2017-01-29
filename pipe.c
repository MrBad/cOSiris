#include <string.h>
#include "pipe.h"
#include "vfs.h"
#include "kheap.h"
#include "task.h"
#include "console.h"
#include "x86.h"


//////////////////////////////////////////////////
// TODO - destroy pipe, spinlock
// Need a clean way to schedule next, wait queue,
// put process to sleep -> some clean interface ...
// maibe a generic queue or something
//////////////////////////////////////////////////
static void awake_list(vfs_pipe_t *pipe)
{
	cli();
	node_t *n;
	for(n = pipe->wait_queue->head; n; n = n->next) {
		task_t *t = (task_t *) n->data;
		t->state = TASK_READY;
		list_del(pipe->wait_queue, n);
		kprintf("waking up writing task:%d\n", t->pid);
	}
	sti();
}
void pipe_open(fs_node_t *node, unsigned int flags) {
	vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
	if(flags == PIPE_READ) {
		pipe->readers++;
	}
	if(flags == PIPE_WRITE) {
		pipe->writers++;
	}
	// kprintf("r:%d, w:%d\n", pipe->readers, pipe->writers);
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
		cli();
		if(!pipe->writers) {
			kprintf("No writers on pipe\n");
			buffer[bytes++] = 0;
			sti();
			return bytes;
		}
		while(pipe->write_pos > pipe->read_pos && bytes < size) {
			buffer[bytes] = pipe->buffer[pipe->read_pos % pipe->size];
			bytes++; pipe->read_pos++;
		}
		sti();

		awake_list(pipe);

		if(bytes == 0) {
			// add me to the waiting q //
			list_add(pipe->wait_queue, current_task);
			// put me to sleep and switch task //

			cli();
			kprintf("sleeping: %d on read\n", current_task->pid);
			current_task->state = TASK_SLEEPING;
			sti();

			task_switch();
		}
	}
	return bytes;
}

unsigned int pipe_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
	// kprintf("pipewrite r:%d, w:%d\n", pipe->readers, pipe->writers);
	unsigned int bytes = 0;
	while(bytes < size) {
		cli();
		if(!pipe->readers) {
			kprintf("No readers on pipe\n");
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
		sti();

		awake_list(pipe);

		if(bytes < size) {
			// add me to the waiting q //
			list_add(pipe->wait_queue, current_task);
			// put me to sleep and switch task //

			cli();
			kprintf("sleeping: %d on write\n", current_task->pid);
			current_task->state = TASK_SLEEPING;
			sti();

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
	pipe->wait_queue = list_open(0);

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
