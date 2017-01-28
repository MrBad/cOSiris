#include <string.h>
#include "vfs.h"
#include "initrd.h"
#include "kheap.h"
#include "console.h"

unsigned int read_null(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
	(void) node; (void) offset; (void) size; (void) buffer;
	return 0;
}
unsigned int write_null(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
	(void) node; (void) offset; (void) size; (void) buffer;
	return 0;
}
void open_null(struct fs_node *node, unsigned int flags) {
	(void) node; (void) flags;
	return;
}
void close_null(struct fs_node *node) {
	(void) node;
	return;
}

unsigned int read_zero(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
	(void) node; (void) offset;
	memset(buffer+offset, 0, size);
	return size;
}
unsigned int write_zero(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
	(void) node; (void) offset; (void) size; (void) buffer;
	return 0;
}
void open_zero(struct fs_node *node, unsigned int flags) {
	(void) node; (void) flags;
	return;
}
void close_zero(struct fs_node *node) {
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
	n->read = read_null;
	n->write = write_null;
	n->open = open_null;
	n->close = close_null;
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
	n->read = read_zero;
	n->write = write_zero;
	n->open = open_zero;
	n->close = close_zero;
	n->readdir = 0;
	n->finddir = 0;
	return n;
}

#if 0
void zero_init()
{
	fs_node_t *dev = finddir_fs(fs_root, "dev");
	// kprintf("dev->inode:%d\n", dev->inode);
	fs_node_t *n = create_null_device();
	n->parent_inode = dev->inode;
	fs_node_t *z = create_zero_device();
	z->parent_inode = dev->inode;
	initrd_add_node(n); //
	initrd_add_node(z); //
	free(n); free(z);

	// fs_node_t *x;
	// x = namei("/dev/zero");
	// if(x) {
	// 	kprintf("%s\n", x->name);
	// 	char buff[256];
	// 	strcpy(buff, "TESTING THIS");
	// 	unsigned int x = read_fs(z, 3, 2, buff);
	// 	kprintf("%d, %s\n", x, buff); // should be TES
	// }
	// x = namei("///dev//zero");
	// if(x) {
	// 	kprintf("\t\tfound: %s-%i\n", x->name, x->inode);
	// }
}
#endif

void zero_init()
{
	fs_node_t *n = create_null_device();
	fs_node_t *z = create_zero_device();
	mount_fs("/dev/null", n);
	mount_fs("/dev/zero", z);
}
