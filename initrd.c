#include "include/string.h"
#include "x86.h"
#include "console.h"
#include "kheap.h"
#include "vfs.h"


typedef struct {
	unsigned int nfiles;
} initrd_header_t;

typedef struct {
	unsigned char magic;
	char name[64];
	unsigned int offset;    // offs into initrd where file starts
	unsigned int length;
} initrd_file_header_t;


initrd_header_t *initrd_header;     // The header.
initrd_file_header_t *file_headers; // The list of file headers.
fs_node_t *initrd_root;             // Our root directory node.
fs_node_t *initrd_dev;              // We also add a directory node for /dev, so we can mount devfs later on.
fs_node_t *root_nodes;              // List of file nodes.
unsigned int nroot_nodes;                    // Number of file nodes.

struct dirent dirent;


unsigned int initrd_read(struct fs_node *node, unsigned int offset, unsigned int size, char *buffer) {
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

struct dirent *initrd_readdir(struct fs_node *node, unsigned int index) {
	if (node == initrd_root && index == 0) {
		strcpy(dirent.name, "dev");
		dirent.name[3] = 0;
		dirent.inode = 0;
		return &dirent;
	}
	if(index-1 >=nroot_nodes) {
		return 0;
	}
	strcpy(dirent.name, root_nodes[index-1].name);
	dirent.name[strlen(root_nodes[index-1].name)] = 0;
	dirent.inode = root_nodes[index-1].inode;
	return &dirent;
}

struct fs_node *initrd_finddir(struct fs_node *node, char *name) {
	if(node == initrd_root && !strcmp(name, "dev")) {
		return initrd_dev;
	}
	unsigned int i;
	for (i = 0; i < nroot_nodes; i++) {
		if(!strcmp(name, root_nodes[i].name)) {
			return &root_nodes[i];
		}
	}
	return 0;
}


fs_node_t *initrd_init(unsigned int location) {
	static bool initrd_inited = false;
	if(initrd_inited) {
		return initrd_root;
	}

	kprintf("Initrd init\n");
	// initialise the root node
	initrd_root = (fs_node_t *) malloc(sizeof(fs_node_t));
	strcpy(initrd_root->name, "initrd");
	initrd_root->mask = initrd_root->uid = initrd_root->gid = initrd_root->length = initrd_root->inode = 0;
	initrd_root->flags = FS_DIRECTORY;
	initrd_root->read = 0;
	initrd_root->write = 0;
	initrd_root->open = 0;
	initrd_root->close = 0;
	initrd_root->readdir = &initrd_readdir;
	initrd_root->finddir = &initrd_finddir;
	initrd_root->ptr = 0;
	initrd_root->impl = 0;

	// initialise /dev node //
	initrd_dev = (fs_node_t *) malloc(sizeof(fs_node_t));
	strcpy(initrd_dev->name, "dev");
	initrd_dev->mask = initrd_dev->uid = initrd_dev->gid = initrd_dev->length = initrd_dev->inode = 0;
	initrd_dev->flags = FS_DIRECTORY;
	initrd_dev->read = 0;
	initrd_dev->write = 0;
	initrd_dev->open = 0;
	initrd_dev->close = 0;
	initrd_dev->readdir = &initrd_readdir;
	initrd_dev->finddir = &initrd_finddir;
	initrd_dev->ptr = 0;
	initrd_dev->impl = 0;

	initrd_header = (initrd_header_t *) location;
	file_headers = (initrd_file_header_t *) (location + sizeof(initrd_header_t));
	// kprintf("%d, %d\n", sizeof(fs_node_t), initrd_header->nfiles);
	// return NULL;

	root_nodes = (fs_node_t *) malloc(sizeof(fs_node_t) * initrd_header->nfiles);
	nroot_nodes = initrd_header->nfiles;

	//for every file
	unsigned int i;
	for (i = 0; i < initrd_header->nfiles; i++) {
		file_headers[i].offset += location;
		// kprintf("^%i, %p, %p\n", i, file_headers[i].offset, location);
		strcpy(root_nodes[i].name, file_headers[i].name);
		root_nodes[i].mask = root_nodes[i].uid = root_nodes[i].gid = 0;
		root_nodes[i].length = file_headers[i].length;
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

	initrd_inited = true;
	return initrd_root;
}
