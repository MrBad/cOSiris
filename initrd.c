#include <stdlib.h>
#include "include/string.h"
#include "x86.h"
#include "console.h"
//#include "kheap.h"
#include "vfs.h"
#include "task.h"


typedef struct {
	unsigned int nfiles;
} initrd_header_t;

typedef struct {
	unsigned int magic;
	char name[64];
	unsigned int offset;    // offs into initrd where file starts
	unsigned int length;
} initrd_file_header_t;


initrd_header_t *initrd_header;		// The header.
initrd_file_header_t *file_headers;	// The list of file headers.
fs_node_t *initrd_root;				// Our root directory node.
// fs_node_t *initrd_dev;				// We also add a directory node for /dev, so we can mount devfs later on.
fs_node_t *root_nodes;				// List of file nodes.
unsigned int nroot_nodes;			// Number of file nodes.
unsigned int allocated_nodes;		// Number of currently alocated nodes > nroot_nodes
struct dirent dirent;


unsigned int initrd_read(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer)
{
	initrd_file_header_t header = file_headers[node->inode];
	if (offset > header.length) {
		return 0;
	}
	if (offset + size > header.length) {
		size = header.length - offset;
	}
	memcpy(buffer, (char *) header.offset + offset, size);
	return size;
}

//unsigned int initrd_write(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer);

//void initrd_open(struct fs_node *node, unsigned int flags);

//void initrd_close(struct fs_node *node);

struct dirent *initrd_readdir(struct fs_node *parent_node, unsigned int index)
{
	if(index > nroot_nodes - 1) {
		return NULL;
	}
	unsigned int i, j;
	for(i = 0, j = 0; i < nroot_nodes; i++) {
		if(root_nodes[i].parent_inode == parent_node->inode) {
			if(j == index) {
				strcpy(dirent.name, root_nodes[i].name);
				dirent.name[strlen(root_nodes[i].name)] = 0;
				dirent.inode = root_nodes[i].inode;
				return &dirent;
			}
			j++;
		}
	}
	return NULL;
}


struct fs_node * initrd_finddir(struct fs_node *parent_node, char *name) {
	unsigned int i;
	// if(!strcmp(name, ".")) {
		// return parent_node;
	// }
	// if(!strcmp(name, "..")) {
	// 	for (i = 0; i < nroot_nodes; i++) {
	// 		if(root_nodes[i].inode == parent_node->parent_inode) {
	// 			return &root_nodes[i];
	// 		}
	// 	}
	// } else {
		for (i = 0; i < nroot_nodes; i++) {
			if(!strcmp(name, root_nodes[i].name) && parent_node->inode == root_nodes[i].parent_inode) {
				return &root_nodes[i];
			}
		}
	// }
	return 0;
}




struct fs_node * initrd_new_node()
{
	fs_node_t *n;
	if(allocated_nodes <= nroot_nodes) {
		// kprintf("Extend array\n");
		root_nodes = (fs_node_t *) realloc(root_nodes, sizeof(fs_node_t) * (allocated_nodes * 2));
	}
	allocated_nodes *= 2;
	n = &root_nodes[nroot_nodes];
	n->inode = nroot_nodes++;
	return n;
}

struct fs_node *initrd_add_node(struct fs_node *node)
{
	if(allocated_nodes <= nroot_nodes) {
		kprintf("Extend array\n");
		root_nodes = (fs_node_t *) realloc(root_nodes, sizeof(fs_node_t) * (allocated_nodes * 2));
	}

	allocated_nodes *= 2;

	struct fs_node *n = &root_nodes[nroot_nodes];
	strcpy(n->name, node->name);
	n->mask = node->mask;
	n->uid = node->uid;
	n->gid = node->gid;
	n->flags = node->flags;
	n->inode = nroot_nodes;
	n->size = node->size;
	n->impl = node->impl;
	n->parent_inode = node->parent_inode;
	n->next = node->next;
	n->read = initrd_read;
	n->write = node->write;
	n->open = node->open;
	n->close = node->close;
	n->readdir = node->readdir;
	n->finddir = node->finddir;
	n->ptr = node->ptr;
	nroot_nodes++;
	return node;
}

struct fs_node * initrd_mkdir(struct fs_node *parent_node, char *name, unsigned int mode)
{
	fs_node_t *n = initrd_new_node();
	strcpy(n->name, name);
	n->mask = mode;
	n->uid = current_task->uid;
	n->gid = current_task->gid;
	n->flags = FS_DIRECTORY;
	n->size = 0;
	n->impl = 0;
	n->parent_inode = parent_node->inode;
	n->next = 0;
	n->read = 0;
	n->write = 0;
	n->open = 0;
	n->close = 0;
	n->readdir = &initrd_readdir;
	n->finddir = &initrd_finddir;
	n->mkdir = &initrd_mkdir;
	n->ptr = 0;
	return n;
}

void initrd_dump() {
	unsigned int i;
	for(i=0; i<nroot_nodes; i++) {
		kprintf("i:%i, inode:%i, parent_inode:%i, %s\n", i, root_nodes[i].inode, root_nodes[i].parent_inode, root_nodes[i].name);
	}
}

fs_node_t *initrd_init(unsigned int location)
{
	static bool initrd_inited = false;
	if(initrd_inited) {
		return initrd_root;
	}

	kprintf("Initrd init\n");
	// initialise the root node
	initrd_root = (fs_node_t *) calloc(1, sizeof(fs_node_t));
	strcpy(initrd_root->name, "/");
	initrd_root->mask = initrd_root->uid = initrd_root->gid = initrd_root->size = initrd_root->inode = 0;
	initrd_root->flags = FS_DIRECTORY;
	initrd_root->read = 0;
	initrd_root->write = 0;
	initrd_root->open = 0;
	initrd_root->close = 0;
	initrd_root->readdir = &initrd_readdir;
	initrd_root->finddir = &initrd_finddir;
	initrd_root->mkdir = &initrd_mkdir;
	initrd_root->ptr = 0;
	initrd_root->impl = 0;

	initrd_header = (initrd_header_t *) location;
	file_headers = (initrd_file_header_t *) (location + sizeof(initrd_header_t));
	// kprintf("fs_node_t: %d, numfiles: %d\n", sizeof(fs_node_t), initrd_header->nfiles);
	// return NULL;

	root_nodes = (fs_node_t *) calloc(1 + initrd_header->nfiles, sizeof(fs_node_t));
	nroot_nodes = initrd_header->nfiles;

	//for every file
	unsigned int i;
	for (i = 0; i < initrd_header->nfiles; i++) {
		file_headers[i].offset += location;
		// kprintf("^%i, %p, %p\n", i, file_headers[i].offset, location);
		strcpy(root_nodes[i].name, file_headers[i].name);
		root_nodes[i].mask = root_nodes[i].uid = root_nodes[i].gid = root_nodes[i].parent_inode = 0;
		root_nodes[i].size = file_headers[i].length;
		root_nodes[i].inode = i;
		root_nodes[i].flags = FS_FILE;
		root_nodes[i].read = &initrd_read;
		root_nodes[i].write = 0;
		root_nodes[i].readdir = 0;
		root_nodes[i].finddir = 0;
		root_nodes[i].open = 0;
		root_nodes[i].close = 0;
		root_nodes[i].impl = 0;
	}

	// initialise /dev node //
	strcpy(root_nodes[i].name, "dev");
	root_nodes[i].inode = i;
	root_nodes[i].flags = FS_DIRECTORY;
	root_nodes[i].readdir = &initrd_readdir;
	root_nodes[i].finddir = &initrd_finddir;
	root_nodes[i].mkdir = &initrd_mkdir;
	i++;

	nroot_nodes  = allocated_nodes = i;
	initrd_inited = true;

	return initrd_root;
}
