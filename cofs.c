//
//	cOSiris *nix like file system
//
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "assert.h"
#include "console.h"
#include "hd.h"
#include "hd_queue.h"
#include "cofs.h"
#include "list.h"
#include "vfs.h"

struct cofs_superblock superb;
#define MAX_CACHED_NODES 1000
#define COFS_ROOT_INUM 1

typedef struct {
	spin_lock_t lock;
	list_t *list;
} cofs_cache_t;

cofs_cache_t *cofs_cache;



void cofs_open(fs_node_t *node, unsigned int flags);
void cofs_close(fs_node_t *node);
unsigned int cofs_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);
unsigned int cofs_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);
fs_node_t *cofs_finddir(fs_node_t *node, char *name);
struct dirent *cofs_readdir(fs_node_t *node, unsigned int index);
fs_node_t *cofs_mkdir(fs_node_t *node, char *name, unsigned int mode);

fs_node_t *cofs_get_node(uint32_t inum);



void get_superblock(cofs_superblock_t *sb)
{
	hd_buf_t *hdb = get_hd_buf(1);
	memcpy(sb, hdb->buf, sizeof(struct cofs_superblock));
	put_hd_buf(hdb);
}

// zeroes a block on disk //
void block_zero(uint32_t block)
{
	hd_buf_t *hdb = get_hd_buf(block);
	hdb->is_dirty = true;
	memset(hdb->buf, 0, sizeof(hdb->buf));
	put_hd_buf(hdb);
}

// finds first unalocated block //
uint32_t block_alloc()
{
	hd_buf_t *hdb;
	uint32_t block, scan, idx, mask, iterations=0;
	for(block = 0; block < superb.size; block+=BITS_PER_BLOCK) {
		hdb = get_hd_buf(BITMAP_BLOCK(block, superb));
		// fast farword using big index until find a non full int //
		for(scan = 0; scan < BLOCK_SIZE/sizeof(uint32_t); scan++,iterations++) {
			if(((uint32_t *)hdb->buf)[scan] != 0xFFFFFFFF) {
				break;
			}
		}
		if(scan != BLOCK_SIZE/sizeof(uint32_t)) {
			for(idx=scan*32; idx < (scan+1)*32; idx++,iterations++) {
				mask = 1 << (idx % 8);
				if((hdb->buf[idx / 8] & mask) == 0) {
					hdb->buf[idx / 8] |= mask;
					hdb->is_dirty = true;
					put_hd_buf(hdb);
					block_zero(idx);
					return (block+idx);
				}
			}
		}
		put_hd_buf(hdb);
	}
	panic("Cannot find any free block");
	return 0;
}

// frees a block number on disk //
void block_free(uint32_t block)
{
	// kprintf("free block %d\n",block);
	hd_buf_t *hdb;
	uint32_t bitmap_block, idx, mask;
	bitmap_block = BITMAP_BLOCK(block, superb);
	hdb = get_hd_buf(bitmap_block);
	idx = block % BITS_PER_BLOCK;
	mask = 1 << (idx % 8);
	if((hdb->buf[idx/8] & mask) == 0) {
		panic("Block %d allready free", block);
	}
	hdb->buf[idx/8] &= ~mask;
	hdb->is_dirty = true;
	put_hd_buf(hdb);
}

//	Allocates a free inode on disk
fs_node_t *inode_alloc(unsigned short int type)
{
	int ino_num;
	hd_buf_t *hdb;
	cofs_inode_t *ino;
	for(ino_num = 1; ino_num < superb.num_inodes; ino_num++) {
		hdb = get_hd_buf(INO_BLK(ino_num, superb));
		ino = (cofs_inode_t*)hdb->buf + ino_num % NUM_INOPB;
		if(ino->type == 0) { // if type == 0 -> is unalocated
			memset(ino, 0, sizeof(*ino));
			ino->type = type;
			hdb->is_dirty = true;
			put_hd_buf(hdb);
			return cofs_get_node(ino_num);
		}
		put_hd_buf(hdb);
	}
	panic("No free inodes\n");
	return NULL;
}


// update from memory inode on disk //
void cofs_update_node(fs_node_t *node)
{
	cofs_inode_t *ino;
	hd_buf_t *hdb = get_hd_buf(INO_BLK(node->inode, superb));
	ino = (cofs_inode_t *) hdb->buf + node->inode % NUM_INOPB;
	ino->type = node->type;
	ino->type = node->type;
	ino->uid = node->uid;
	ino->gid = node->gid;
	ino->num_links = node->num_links;
	ino->atime = node->atime;
	ino->ctime = node->ctime;
	ino->mtime = node->mtime;
	ino->size = node->size;
	memcpy(ino->addrs, node->addrs, sizeof(ino->addrs));
	hdb->is_dirty = true;
	put_hd_buf(hdb);
}

//	finds the inode inum and returns a cached fs_node
// 	does not retrieve the inode from disk - lock does that
fs_node_t *cofs_get_node(uint32_t inum)
{
	node_t *n;
	fs_node_t *node, *free_node = NULL;
	spin_lock(&cofs_cache->lock);
	for(n = cofs_cache->list->head; n; n = n->next) {
		node = (fs_node_t *)n->data;
		if(node->inode == inum) {
			node->ref_count++;
			spin_unlock(&cofs_cache->lock);
			return node;
		}
		if(node->ref_count == 0) { // unused by any process
			free_node = node;
		}
	}
	if(!free_node) {
		if(cofs_cache->list->num_items >= MAX_CACHED_NODES) {
			kprintf("cofs_get_node - MAX_CACHED_NODES limit\n");
			return NULL;
		}
		free_node = malloc(sizeof(fs_node_t));
		list_add(cofs_cache->list, free_node);
	}
	memset(free_node, 0, sizeof(*free_node));
	node = free_node;
	node->inode = inum;
	node->ref_count = 1;
	// ops //
	node->open = cofs_open;
	node->close = cofs_close;
	node->read = cofs_read;
	node->write = cofs_write;
	node->finddir = cofs_finddir;
	node->readdir = cofs_readdir;
	node->mkdir = cofs_mkdir;
	spin_unlock(&cofs_cache->lock);
	return node;
}

fs_node_t *cofs_dup(fs_node_t *node)
{
	spin_lock(&cofs_cache->lock);
	node->ref_count++;
	spin_unlock(&cofs_cache->lock);
	return node;
}

//locks the inode,
//gets data to it from disk if needed
void cofs_lock(fs_node_t *node)
{
	hd_buf_t *hdb;
	if(node == NULL||node->ref_count == 0) {
		if(node == NULL)
			panic("cofs_lock - no node\n");
		else
			panic("ref_count for node %s is %d\n", node->name, node->ref_count);
	}
	spin_lock(&node->lock);
	if(node->type == 0) {
		//kprintf("geting from disk: %d\n", node->inode);
		hdb = get_hd_buf(INO_BLK(node->inode, superb));
		cofs_inode_t *ino = (cofs_inode_t*) hdb->buf + node->inode % NUM_INOPB;
		node->type = node->flags = ino->type;
		node->uid = ino->uid;
		node->gid = ino->gid;
		node->num_links = ino->num_links;
		node->atime = ino->atime;
		node->ctime = ino->ctime;
		node->mtime = ino->mtime;
		node->size = ino->size;
		node->addrs = calloc(1, sizeof(ino->addrs));
		memcpy(node->addrs, ino->addrs, sizeof(ino->addrs));
		put_hd_buf(hdb);
		if(node->type == 0) {
			panic("cofs_lock - node type is 0");
		}
	}
}

void cofs_unlock(fs_node_t *node)
{
	if(node==NULL || node->lock !=1 || node->ref_count < 0) {
		kprintf("cofs_unlock()");
		if(node == NULL)
			kprintf("node is null\n");
		if(node->lock != 1)
			kprintf("node not locked\n");
		if(node->ref_count < 0)
			kprintf("ref_count < 0\n");
		panic("");
	}
	spin_unlock(&node->lock);
}

// truncate the node //
void cofs_trunc(node){}

// release the node - if no num_links reference it -> truncate it //
void cofs_put_node(fs_node_t *node)
{
	spin_lock(&cofs_cache->lock);
	if(node->ref_count==1 && node->type > 0 && node->num_links == 0) {
		spin_unlock(&cofs_cache->lock);
		cofs_trunc(node);
		node->type = 0;
		cofs_update_node(node);
		spin_lock(&cofs_cache->lock);
	}
	node->ref_count--;
	spin_unlock(&cofs_cache->lock);
}

//
//	 fbn - file block number 0,1,2...
//	return disk block address of nth block in inode ino
//	if there is no such block, it allocates one
//
uint32_t block_map(fs_node_t *node, uint32_t fbn)
{
	hd_buf_t *hdb;
	uint32_t addr;

	if(fbn < NUM_DIRECT) {
		if(node->addrs[fbn] == 0) {
			node->addrs[fbn] = block_alloc();
		}
		return node->addrs[fbn];
	}
	else if(fbn < NUM_DIRECT+NUM_SIND) {
		// kprintf("indirect\n");
		if(node->addrs[SIND_IDX] == 0) {
			node->addrs[SIND_IDX] = block_alloc();
		}
		hdb = get_hd_buf(node->addrs[SIND_IDX]);
		if(((uint32_t *)hdb->buf)[fbn-NUM_DIRECT] == 0) {
			((uint32_t *)hdb->buf)[fbn-NUM_DIRECT] = block_alloc();
			hdb->is_dirty = true;
		}
		addr = ((uint32_t *)hdb->buf)[fbn-NUM_DIRECT];
		put_hd_buf(hdb);
		return addr;
	}
	else if(fbn < NUM_DIRECT+NUM_SIND+NUM_DIND) {
		// kprintf("double indirect\n");
		uint32_t midx = (fbn-NUM_DIRECT-NUM_SIND) / NUM_SIND;
		uint32_t sidx = (fbn-NUM_DIRECT-NUM_SIND) % NUM_SIND;
		hdb = get_hd_buf(node->addrs[DIND_IDX]);
		if(((uint32_t*)hdb->buf)[midx] == 0) {
			((uint32_t*)hdb->buf)[midx] = block_alloc();
			hdb->is_dirty = true;
		}
		uint32_t mblk = ((uint32_t*)hdb->buf)[midx];
		put_hd_buf(hdb);
		hdb = get_hd_buf(mblk);
		if(((uint32_t*)hdb->buf)[sidx] == 0) {
			((uint32_t*)hdb->buf)[sidx] = block_alloc();
			hdb->is_dirty = true;
		}
		addr = ((uint32_t*)hdb->buf)[sidx];
		put_hd_buf(hdb);
		return addr;
	}
	panic("block_map out of range\n");
	return 0;
}

#ifndef min
	#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

// read max size bytes from node offset into buffer //
unsigned int cofs_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	hd_buf_t *hdb;
	if(offset > node->size) {
		return 0;
	}
	if(offset + size > node->size) {
		size = node->size - offset;
	}
	unsigned int total, num_bytes;
	for(total = 0; total < size; total += num_bytes) {
		hdb = get_hd_buf(block_map(node, offset / BLOCK_SIZE));
		num_bytes = min(size - total, BLOCK_SIZE - offset % BLOCK_SIZE);
		memcpy(buffer, hdb->buf + offset % BLOCK_SIZE, num_bytes);
		offset += num_bytes;
		buffer += num_bytes;
		put_hd_buf(hdb);
	}
	return size;
}
// writes size bytes from buffer, at offset in node //
unsigned int cofs_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
	hd_buf_t *hdb;
	unsigned int total, num_bytes = 0;

	if(offset > node->size) {
		return 0;
	}
	if(offset + size > MAX_FILE_SIZE*BLOCK_SIZE) {
		return 0;
	}
	for(total = 0; total < size; total += num_bytes) {
		hdb = get_hd_buf(block_map(node, offset / BLOCK_SIZE));
    	num_bytes = min(size - total, BLOCK_SIZE - offset % BLOCK_SIZE);
		memcpy(hdb->buf + offset % BLOCK_SIZE, buffer, num_bytes);
		offset += num_bytes;
		buffer += num_bytes;
		hdb->is_dirty=true;
	    put_hd_buf(hdb);
  	}
	kprintf("write %s, offs: %d, size:%d, node_size: %d\n", node->name, offset, size, node->size);
	if(size > 0 && offset > node->size) {
		kprintf("update node\n");
	  	node->size = offset;
		cofs_update_node(node);
	}
	return size;
}

void cofs_open(fs_node_t *node, unsigned int flags) {
	cofs_lock(node);// we should lock only if writing?!
}
void cofs_close(fs_node_t *node) {
	if(node->lock) cofs_unlock(node);
	cofs_put_node(node);
}

// find and retrieve node having name, inside parent node directory //
fs_node_t *cofs_finddir(fs_node_t *node, char *name)
{
	if(!(node->type & FS_DIRECTORY)) {
		panic("In search for %s - node: %s is not a directory\n", name, node->name);
	}

	cofs_dirent_t dir;
	int n, offs = 0, inum = 0;
	fs_node_t *new_node;

	while((n = cofs_read(node, offs, sizeof(dir), (char *)&dir)) > 0) {
		if(strcmp(dir.name, name) == 0) {
			inum = dir.inode;
			break;
		}
		offs += n;
	}
	if(inum == 0) {
		return NULL;
	}
	new_node = cofs_get_node(inum);
	cofs_lock(new_node);
	strncpy(new_node->name, name, sizeof(new_node->name)-1);
	cofs_unlock(new_node);
	// cofs_put_node(new_node);
	return new_node;
}

struct dirent dirent;
struct dirent *cofs_readdir(fs_node_t *node, unsigned int index)
{
	unsigned int n, offs;
	cofs_dirent_t dir;

	cofs_lock(node);
skip:
	offs = index * sizeof(cofs_dirent_t);
	if((n = cofs_read(node, offs, sizeof(dir), (char *)&dir)) > 0) {
		if(dir.inode == 0) {
			index++;
			goto skip;
		}
		strncpy(dirent.name, dir.name, sizeof(dirent.name)-1);
		dirent.inode = dir.inode;
	} else {
		cofs_unlock(node);
		return NULL;
	}
	cofs_unlock(node);
	return &dirent;
}

// link - insert name and inode into directory node //
int cofs_dirlink(fs_node_t *node, char *name, unsigned int inum)
{
	fs_node_t *n;
	if((n = cofs_finddir(node, name))!= NULL) {
		kprintf("cofs_dirlink - file %s exists\n", name);
		cofs_put_node(n);
		return -1;
	}
	int num_bytes;
	cofs_dirent_t dir = {0};
	unsigned int offs = 0;
 	while((num_bytes = cofs_read(node, offs, sizeof(dir), (char *)&dir)) > 0) {
		if(dir.inode == 0) { // recycle old node
			break;
		}
		offs += num_bytes;
	}
	strncpy(dir.name, name, sizeof(dir.name));
	dir.inode = inum;
	if((cofs_write(node, offs, sizeof(dir), (char *)&dir)) != sizeof(dir)) {
		panic("cofs_dirlink");
	}
	return 0;
}

// creates a directory in node
fs_node_t * cofs_mkdir(fs_node_t *node, char *name, unsigned int mode)
{
	kprintf("mkdir: %d\n", node->inode);
	fs_node_t *n;

	if((n = cofs_finddir(node, name))!= NULL) {
		cofs_put_node(n);
		kprintf("directory %s exists\n", name);
		return NULL;
	}
	fs_node_t *new_dir = inode_alloc(FS_DIRECTORY);
	cofs_lock(new_dir);
	// kprintf("links: %d\n", new_dir->num_links);
	new_dir->mask = mode; // and mask?! //
	new_dir->num_links = 1;
	// create . and .. //
	cofs_dirlink(new_dir, ".", new_dir->inode); // self
	cofs_dirlink(new_dir, "..", node->inode); // parent
	cofs_update_node(new_dir);
	cofs_unlock(new_dir);

	cofs_lock(node);
	cofs_dirlink(node, name, new_dir->inode); // link it to parent
	cofs_unlock(node);

	return new_dir;
}

void cofs_dump_cache()
{
	node_t *n;
	fs_node_t *node;
	spin_lock(&cofs_cache->lock);
	kprintf("%d nodes in cache\n", cofs_cache->list->num_items);
	for(n = cofs_cache->list->head; n; n = n->next) {
		node = (fs_node_t *)n->data;
		kprintf("name: %s, ino: %d, ref_count: %d\n", node->name, node->inode, node->ref_count);
	}
	spin_unlock(&cofs_cache->lock);
}

// cofs init - returns root fs node //
fs_node_t *cofs_init()
{
	fs_node_t *root;

	KASSERT(BLOCK_SIZE % sizeof(struct cofs_inode) == 0);
	KASSERT(BLOCK_SIZE % sizeof(cofs_dirent_t) == 0);
	kprintf("sizeof inode: %d\n",sizeof(cofs_inode_t));

	hd_queue_init();

	get_superblock(&superb);
	if(superb.magic != COFS_MAGIC) {
		panic("Invalid file system, cannot find COFS_MAGIC\n");
	}

	kprintf("size: %d, data: %d, num_inodes:%d, bitmap_start: %d, ino_start: %d\n", superb.size, superb.num_blocks, superb.num_inodes,
		superb.bitmap_start, superb.inode_start);
	kprintf("Max file size: %d KB\n", MAX_FILE_SIZE * 512 / 1024);

	cofs_cache = calloc(1, sizeof(*cofs_cache));
	cofs_cache->list = list_open(NULL);

	root = cofs_get_node(COFS_ROOT_INUM);
	cofs_lock(root);
	if(root->type != FS_DIRECTORY) {
		panic("root is not a directory");
	}
	strcpy(root->name, "/");
	cofs_dup(root); // increase ref_count so we never discard it from cache
	cofs_unlock(root);
	cofs_put_node(root);

	// check /dev
	fs_node_t *dev = cofs_finddir(root, "dev");
	if(!dev) {
		cofs_mkdir(root, "dev", 0755);
	}
	// create /dev/console
	fs_node_t *console = cofs_finddir(dev, "console");
	if(!console) {
		console = inode_alloc(FS_CHARDEVICE);
		cofs_dirlink(dev, "console", console->inode);
	}
	kprintf("dev: %d\n", dev->size);
	return root;
}
