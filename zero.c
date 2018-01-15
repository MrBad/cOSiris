#include <string.h>
#include "vfs.h"
#include "initrd.h"
#include "kheap.h"
#include "console.h"

unsigned int null_read(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
    (void) node; (void) offset; (void) size; (void) buffer;
    return 0;
}
unsigned int null_write(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
    (void) node; (void) offset; (void) size; (void) buffer;
    return 0;
}
void null_open(struct fs_node *node, unsigned int flags) {
    (void) node; (void) flags;
    return;
}
void null_close(struct fs_node *node) {
    (void) node;
    return;
}

unsigned int zero_read(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
    (void) node; (void) offset;
    memset(buffer+offset, 0, size);
    return size;
}
unsigned int zero_write(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
    (void) node; (void) offset; (void) size; (void) buffer;
    return 0;
}
void zero_open(struct fs_node *node, unsigned int flags) {
    (void) node; (void) flags;
    return;
}
void zero_close(struct fs_node *node) {
    (void) node;
    return;
}


fs_node_t *create_null_device()
{
    // fs_node_t * n = (fs_node_t *) calloc(sizeof(fs_node_t));
    // n->inode = 0;
    fs_node_t *n = initrd_new_node();
    strcpy(n->name, "null");
    n->uid = n->gid = 0;
    n->mask = 0666;
    n->flags = FS_CHARDEVICE;
    n->read = null_read;
    n->write = null_write;
    n->open = null_open;
    n->close = null_close;
    n->readdir = 0;
    n->finddir = 0;
    return n;
}

fs_node_t *create_zero_device()
{
    // fs_node_t * n = (fs_node_t *) calloc(sizeof(fs_node_t));
    // n->inode = 0;
    fs_node_t * n = initrd_new_node();
    strcpy(n->name, "zero");
    n->uid = n->gid = 0;
    n->mask = 0666;
    n->flags = FS_CHARDEVICE;
    n->read = zero_read;
    n->write = zero_write;
    n->open = zero_open;
    n->close = zero_close;
    n->readdir = 0;
    n->finddir = 0;
    return n;
}

void zero_init()
{
    fs_node_t *n = create_null_device();
    fs_node_t *z = create_zero_device();
    fs_mount("/dev/null", n);
    fs_mount("/dev/zero", z);
}
