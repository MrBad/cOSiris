#include "include/types.h"
#include "console.h"
#include "kheap.h"
#include "vfs.h"


unsigned int read_fs(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer) {
	return node->read != 0 ? node->read(node, offset, size, buffer) : 0;
}

unsigned int write_fs(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer) {
	return node->write != 0 ? node->write(node, offset, size, buffer) : 0;
}

void open_fs(fs_node_t *node, unsigned int flags) {
	if (node->open != NULL) {
		return node->open(node, flags);
	}
}

void close_fs(fs_node_t *node) {
	if (node->close != NULL) {
		return node->close(node);
	}
}

struct dirent *readdir_fs(fs_node_t *node, unsigned int index) {
	if ((node->flags & FS_DIRECTORY) && node->readdir != 0) {
		return node->readdir(node, index);
	} else {
		return false;
	}
}

fs_node_t *finddir_fs(fs_node_t *node, char *name) {
	// Is the node a directory, and does it have a callback?
	if ((node->flags & FS_DIRECTORY) && node->finddir != 0)
		return node->finddir(node, name);
	else
		return false;
}

//void initrd_open(fs_node_t *node, unsigned int flags) { }
