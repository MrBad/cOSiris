#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "console.h"
#include "vfs.h"
#include "pipe.h"
#include "task.h"

void pipe_open(fs_node_t *node, unsigned int flags)
{
    vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
    spin_lock(&pipe->lock);
    if (flags == O_RDONLY) {
        if (node->flags != O_RDONLY) {
            panic("read only side of pipe\n");
        }
        node->ref_count++;
        pipe->readers++;
    } 
    else if (flags == O_WRONLY) {
        if (node->flags != O_WRONLY) {
            panic("write only side of pipe\n");
        }
        node->ref_count++;
        pipe->writers++;
    }
    else {
        panic("pipe: unknown open mode %d\n", node->type);
    }
    spin_unlock(&pipe->lock);
}

void pipe_close(fs_node_t *node) 
{
    // kprintf("closing node: %s\n", node->name);
    vfs_pipe_t *pipe = (vfs_pipe_t*) node->ptr;
    spin_lock(&pipe->lock);
    if (node->flags == O_RDONLY) {
        pipe->readers--;
    } else if (node->flags == O_WRONLY) {
        pipe->writers--;
    } else {
        panic("pipe: unknown open mode: %d\n", node->type);
    }
    node->ref_count--;

    if (node->flags == O_RDONLY) 
        wakeup(&pipe->write_pos);
    else
        wakeup(&pipe->read_pos);

    if (node->ref_count ==0) {
        free(node);
    }
    kprintf("r: %d, w: %d\n", pipe->readers, pipe->writers);
    // cosh bug, sometimes i get < 0 grrrr	
    if (pipe->readers <= 0 && pipe->writers <= 0) {
        kprintf("FREE ALL\n");
        free(pipe->buffer);
        spin_unlock(&pipe->lock);
        free(pipe);
        return;
    }
    spin_unlock(&pipe->lock);
}

unsigned int pipe_read(fs_node_t *node, unsigned int offset, 
        unsigned int size, char *buffer) 
{
    (void) offset;
    unsigned int i;
    vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
    if (node->flags != O_RDONLY) {
        panic("pipe side not opened read only\n");
    }
    spin_lock(&pipe->lock);
    while (pipe->read_pos == pipe->write_pos && pipe->writers > 0) {
        spin_unlock(&pipe->lock);
        sleep_on(&pipe->read_pos);
        spin_lock(&pipe->lock);
    }
    for (i = 0; i < size; i++) {
        if (pipe->read_pos == pipe->write_pos) {
            break;
        }
        buffer[i] = pipe->buffer[pipe->read_pos++ % pipe->size];
    }
    wakeup(&pipe->write_pos);
    spin_unlock(&pipe->lock);

    return i;
}

unsigned int pipe_write(fs_node_t *node, unsigned int offset, 
        unsigned int size, char *buffer)
{
    (void) offset;
    unsigned int i;
    vfs_pipe_t *pipe = (vfs_pipe_t *) node->ptr;
    spin_lock(&pipe->lock);
    if (node->flags != O_WRONLY) {
        panic("pipe side not opened write only\n");
    }
    for (i = 0; i < size; i++) {
        while (pipe->write_pos == pipe->read_pos + pipe->size) { // buf full
            if (pipe->readers == 0) {
                spin_unlock(&pipe->lock);
                return 0;
            }
            wakeup(&pipe->read_pos);
            spin_unlock(&pipe->lock);
            sleep_on(&pipe->write_pos);
            spin_lock(&pipe->lock);
        }
        pipe->buffer[pipe->write_pos++ % pipe->size] = buffer[i];
    }
    wakeup(&pipe->read_pos);
    spin_unlock(&pipe->lock);

    return size;
}

int pipe_new(fs_node_t *nodes[2]) 
{
    vfs_pipe_t *pipe = calloc(1, sizeof(vfs_pipe_t));
    pipe->buffer = malloc(PIPE_BUFFER_SIZE);
    pipe->size = PIPE_BUFFER_SIZE;

    nodes[0] = calloc(1, sizeof(fs_node_t));
    nodes[1] = calloc(1, sizeof(fs_node_t));
    strcpy(nodes[0]->name, "read_pipe");
    strcpy(nodes[1]->name, "write_pipe");
    nodes[0]->type = nodes[1]->type = FS_PIPE;
    nodes[0]->flags = O_RDONLY;
    nodes[1]->flags = O_WRONLY;
    nodes[0]->open = nodes[1]->open = pipe_open;
    nodes[0]->close = nodes[1]->close = pipe_close;
    nodes[0]->ptr = nodes[1]->ptr = pipe;
    nodes[0]->read = pipe_read;
    nodes[1]->write = pipe_write;

    return 0;
}
