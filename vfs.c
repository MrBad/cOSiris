#include <string.h>
#include "include/types.h"
#include "console.h"
#include "kheap.h"
#include "assert.h"

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

//
//	Simple name to inode find
//
fs_node_t *namei(char *path)
{
	fs_node_t *node = NULL;
	KASSERT(*path);
	KASSERT(*path == '/');

	char *p; int len = strlen(path);
	if(len == 1) {
		return fs_root;
	}
	char *str = strdup(path);
	for(p = str; *p; p++) {
		if(*p == '/') {
			*p = 0;
		}
	}
	p = str;
	while(len > 0) {
		if(*p) {
			if(!(node = finddir_fs(node ? node : fs_root, p))) {
				break;
			}
		}
		unsigned int curr_len = strlen(p);
		len = len - curr_len - 1;
		p = p + curr_len + 1;
	}
	free(str);
	return node;
}

// needs more testing -> for now will only mount /dev/ files
int mount_fs(char *path, fs_node_t *node)
{
	KASSERT(*path);
	KASSERT(*path == '/');
	fs_node_t *n;
	n = namei(path);
	if(!n) {
		// full path does not exits, check if parent node exists //
		// usually used in mount /dev/xxxx files
		char *dir, *base; // /di/r/base
		int len = strlen(path);
		dir = strdup(path);
		for(base = dir + len - 1; base >= dir; base--) {
			if(*base == '/') {
				*base = 0; base++; break;
			}
		}
		if(strlen(base) && !strcmp(node->name, base)) {
			n = namei(dir);
			if(!n) {
				free(dir);
				return -1;
			}
			// add to parent //
			node->parent_inode = n->inode;
		}
		free(dir);
	}
	else {
		// replace parent //
		node->parent_inode = n->parent_inode;
		node->ptr = n; // keep track of old node - n, so we can unmount later
		node->flags = FS_MOUNTPOINT;
		// node->parent_inode = n->inode;
	}
	// kprintf("%d, %d, %s\n", node->inode, node->parent_inode, node->name);
	return 0;
}
