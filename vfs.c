#include "include/types.h"
#include <string.h>
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

// stdlib realpath,
// libgen dirname, basename,
// simple name to inode - needs rewrite, i don't like it
fs_node_t *namei(char *path)
{
	fs_node_t *node = NULL;
	char *p = path;
	char *d; int i = 0;
	// bool have_new = false;
	d = (char *)malloc(strlen(path)+1);
	while(*p) {
		if(*p == '/') {
			if(i > 0) {
				// kprintf("%s\n", d);
				node = finddir_fs(node ? node : fs_root, d);
				if(!node) {
					return NULL;
				}
			}
			i = 0; p++;
			continue;
		}
		d[i++] = *p++;
		d[i] = 0;
		if(*p == 0) {
			// kprintf("%s\n", d);
		}
	}
	// kprintf("%s\n", d);
	node = finddir_fs(node ? node : fs_root, d);
	if(!node) {
		return NULL;
	}
	// kprintf("node: %d", node->inode);
	free(d);
	return node;
}

void mount_fs(char *path, fs_node_t *root)
{
	kprintf("mount_fs: %s, %s\n", path, root->name);
}
