#include <string.h>
#include <stdlib.h>	// malloc
#include <sys/types.h>
#include <dirent.h>
#include "console.h"
#include "serial.h"
#include "assert.h"
#include "task.h"
#include "vfs.h"
#include "canonize.h"

void fs_open(fs_node_t *node, unsigned int flags)
{
	if(node->open != NULL) {
		return node->open(node, flags);
	}
}

void fs_close(fs_node_t *node)
{
	if (node->close != NULL) {
		return node->close(node);
	}
}

unsigned int fs_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	return node->read != 0 ? node->read(node, offset, size, buffer) : 0;
}

unsigned int fs_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	return node->write != 0 ? node->write(node, offset, size, buffer) : 0;
}


struct dirent *fs_readdir(fs_node_t *node, unsigned int index)
{
	if ((node->type & FS_DIRECTORY) && node->readdir != 0) {
		return node->readdir(node, index);
	} else {
		return false;
	}
}

fs_node_t *fs_finddir(fs_node_t *node, char *name)
{
	// Is the node a directory, and does it have a callback?
	if ((node->type & FS_DIRECTORY) && node->finddir != 0) {
		return node->finddir(node, name);
	} else {
		// kprintf("finddir not found for node: %d\n", node->inode);
		return false;
	}
}


fs_node_t * fs_mkdir(fs_node_t *node, char *name, int mode)
{
	if(!(node->type & FS_DIRECTORY)) {
		return NULL;
	}
	if(fs_finddir(node, name)) {
		return NULL;
	}
	if(!node->mkdir) {
		return NULL;
	}
	return node->mkdir(node, name, mode);
}


fs_node_t *fs_dup(fs_node_t *node)
{
	spin_lock(&node->lock);
	node->ref_count++;
	spin_unlock(&node->lock);
	return node;
}


//
//	Simple name to vfs node find ~ namei
//
fs_node_t *fs_namei(char *path)
{
	fs_node_t *node = NULL;
	KASSERT(*path);
	if(*path != '/') {
		panic("Path [%s] is not full path\n", path);
		return NULL;
	}
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
			if(node) {
				//kprintf("fs_namei - open %s, ref: %d, len: %d\n", node->name, node->ref_count, len);
				if(node->inode!=1) node->ref_count--;
			}

			if(!(node = fs_finddir(node ? node : fs_root, p))) {
				break;
			}
		}
		unsigned int curr_len = strlen(p);
		len = len - curr_len - 1;
		p = p + curr_len + 1;
	}
	serial_debug("free namei str");
	free(str);
	return node;
}


#if 0

// needs more testing -> for now will only mount /dev/ files
int fs_mount(char *path, fs_node_t *node)
{

	KASSERT(*path);
	KASSERT(*path == '/');
	fs_node_t *n;

	n = fs_namei(path);
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
			n = fs_namei(dir);
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
		kprintf("MOUNT\n");
		// replace parent //
		node->parent_inode = n->parent_inode;
		node->ptr = n; // keep track of old node - n, so we can unmount later
		node->flags = FS_MOUNTPOINT;
		// node->parent_inode = n->inode;
	}
	kprintf("%d, %d, %s\n", node->inode, node->parent_inode, node->name);
	return 0;
}
#endif



// hard work here //
// check permissions //
// create if not exists //
// more to do //
int fs_open_namei(char *path, int flags, int mode, fs_node_t **node)
{
	//char buf[512];
	char *p;
	serial_debug("fs_open_namei(%s)\n", path);
	p = canonize_path(current_task->cwd, path);
	//strncpy(buf, p, sizeof(buf)-1);
	//free(p);
	/*if(path[0]=='/') {
		p = strdup(path);
	} else {
		p = malloc(strlen(current_task->cwd)+strlen(path)+2);
		strcpy(p,current_task->cwd);
		strcat(p,"/");
		strcat(p,path);
	}*/
	*node = fs_namei(p);	
	return *node ? 0 : -1;
}


void lstree(fs_node_t *parent, int level)
{
	struct dirent *dir = 0;
	unsigned int i = 0; int j;
	if(!parent) parent = fs_root;
	kprintf("Listing directory: %s\n", parent->name);
	while((dir = fs_readdir(parent, i++))) {
		if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;
		fs_node_t *file = fs_finddir(parent, dir->d_name);
		for(j=0; j<level;j++) kprintf("    ");
		kprintf("%s - inode:%d, \n", file->name, file->inode);
		if(file->type & FS_DIRECTORY) {
			lstree(file, level+1);
		}
		fs_close(file);
	}
}
