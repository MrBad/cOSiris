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

// an in-memory representation of disk inode
typedef struct minode
{
	uint32_t inum;
	int ref_count;
	spin_lock_t lock;
	int flags;

	unsigned short int	type;
	unsigned short int	major;
	unsigned short int	minor;
	unsigned short int	uid;
	unsigned short int	gid;
	unsigned short int	num_links;
	unsigned int atime, mtime, ctime;
	unsigned int size;
	unsigned int addrs[NUM_DIRECT + 3];
} minode_t;


struct cofs_superblock superb;
#define MAX_CACHED_NODES 200
#define COFS_ROOT_INUM 1

typedef struct {
	spin_lock_t lock;
	list_t *list;
} cofs_ncache_t;

cofs_ncache_t *cofs_ncache;


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
cofs_inode_t *inode_alloc(unsigned short int type)
{
	int ino_num;
	hd_buf_t *hdb;
	cofs_inode_t *ino;
	for(ino_num = 1; ino_num < superb.num_inodes; ino_num++) {
		hdb = get_hd_buf(INO_BLK(ino_num, superb));
		ino = (cofs_inode_t*)(hdb->buf + ino_num % NUM_INOPB);
		if(ino->type == 0) { // if type == 0 -> is unalocated
			memset(ino, 0, sizeof(*ino));
			ino->type = type;
			hdb->is_dirty = true;
			put_hd_buf(hdb);
			return ino;
		}
		put_hd_buf(hdb);
	}
	panic("No free inodes\n");
	return NULL;
}

// copy from memory inode to disk
void miupdate(minode_t *minode)
{
	cofs_inode_t *ino;
	hd_buf_t *hdb = get_hd_buf(INO_BLK(minode->inum, superb));
	ino = (cofs_inode_t*) hdb->buf + minode->inum % NUM_INOPB;
	ino->type = minode->type;
	ino->major = minode->major;
	ino->minor = minode->minor;
	ino->num_links = minode->num_links;
	ino->size = minode->size;
	memcpy(ino->addrs, minode->addrs, sizeof(ino->addrs));
	hdb->is_dirty = true;
	put_hd_buf(hdb);
}

//
//	find the inode inum on disk and returns an cached fs_node
//
minode_t *get_minode(uint32_t inum)
{
	node_t *n;
	minode_t *minode, *free_minode=NULL;

	spin_lock(&cofs_ncache->lock);
	for(n = cofs_ncache->list->head; n; n = n->next) {
		minode = (minode_t*)n->data;
		if(minode->inum == inum) {
			spin_unlock(&cofs_ncache->lock);
			return minode;
		}
		if(!free_minode && minode->type == 0) {
			free_minode = minode; // save it //
		}
	}
	if(!free_minode) {
		if(cofs_ncache->list->num_items >= MAX_CACHED_NODES) {
			panic("cached nodes reached MAX_CACHED_NODES %d\n", MAX_CACHED_NODES);
		}
		free_minode = calloc(1, sizeof(minode_t));
		list_add(cofs_ncache->list, free_minode);
	}
	minode = free_minode;
	minode->inum = inum;
	minode->ref_count=1;
	minode->flags = 0;
	kprintf("%d\n", cofs_ncache->list->num_items);
	spin_unlock(&cofs_ncache->lock);
	return minode;
}

minode_t midup(minode_t *minode){}
void mitrunc(minode_t *minode){}

// locks the inode
// gets it from disk if necessary
void milock(minode_t *minode)
{
	if(minode == NULL || minode->ref_count == 0) {
		panic("milock - no minode");
	}
	spin_lock(&minode->lock);
	if(minode->flags == 0) {
		hd_buf_t *hdb = get_hd_buf(INO_BLK(minode->inum, superb));
		cofs_inode_t *ino = (cofs_inode_t*)hdb->buf + minode->inum % NUM_INOPB;
		kprintf("inode size %d\n", ino->size);
		minode->type = ino->type;
		minode->major = ino->major;
		minode->minor = ino->minor;
		minode->uid = ino->uid;
		minode->gid = ino->gid;
		minode->num_links = ino->num_links;
		minode->atime = ino->atime;
		minode->ctime = ino->ctime;
		minode->mtime = ino->mtime;
		minode->size = ino->size;
		memcpy(minode->addrs, ino->addrs, sizeof(minode->addrs));
		put_hd_buf(hdb);
		minode->flags = 1;
		if(minode->type == 0) {
			panic("minode type\n");
		}
	}
}
void miunlock(minode_t *minode)
{
	if(minode==NULL||minode->lock!=1||minode->ref_count<0) {
		panic("miunlock");
	}
	spin_unlock(&minode->lock);
}

void miput(minode_t *minode)
{
	spin_lock(&cofs_ncache->lock);
	if(minode->ref_count==1 && minode->flags&1 && minode->num_links==0) {
		spin_unlock(&cofs_ncache->lock);
		mitrunc(minode);
		minode->type=0;
		miupdate(minode);
		spin_lock(&cofs_ncache->lock);
		minode->flags=0;
	}
	spin_unlock(&cofs_ncache->lock);
}



//
//	 fbn - file block number 0,1,2...
//	return disk block address of nth block in inode ino
//	if there is no such block, it allocates one
//
uint32_t block_map(minode_t *minode, uint32_t fbn)
{
	hd_buf_t *hdb;
	uint32_t addr;

	if(fbn < NUM_DIRECT) {
		if(minode->addrs[fbn] == 0) {
			minode->addrs[fbn] = block_alloc();
		}
		return minode->addrs[fbn];
	}
	else if(fbn < NUM_DIRECT+NUM_SIND) {
		// kprintf("indirect\n");
		if(minode->addrs[SIND_IDX] == 0) {
			minode->addrs[SIND_IDX] = block_alloc();
		}
		hdb = get_hd_buf(minode->addrs[SIND_IDX]);
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
		hdb = get_hd_buf(minode->addrs[DIND_IDX]);
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

void minode_trunc(minode_t *minode)
{
	uint32_t fbn = minode->size % BLOCK_SIZE;
	uint32_t i;
	fbn++;
	for(i=0; i<fbn; i++) {
		kprintf("free block %d\n", fbn);
	}
}
#define min(a, b) ((a) < (b) ? (a) : (b))

int miread(minode_t *minode, char *dest, uint32_t offset, uint32_t size)
{
	uint32_t tot, m;
  	hd_buf_t *hdb;
	// f(ip->type == T_DEV){
	//   if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
	//     return -1;
	//   return devsw[ip->major].read(ip, dst, n);
	// }
	if(offset > minode->size || offset + size < offset)
		return -1;

	if(offset + size > minode->size)
		size = minode->size - offset;

	for(tot=0; tot < size; tot+=m, offset+=m, dest+=m) {
	// kprintf("mIREAD: $%d\n", block_map(minode, offset/BLOCK_SIZE));
		hdb = get_hd_buf(block_map(minode, offset/BLOCK_SIZE));
		m = min(size - tot, BLOCK_SIZE - offset % BLOCK_SIZE);
		memcpy(dest, hdb->buf + offset % BLOCK_SIZE, m);
		put_hd_buf(hdb);
	}
	return size;
}



int cofs_init()
{
	KASSERT(BLOCK_SIZE % sizeof(struct cofs_inode) == 0);
	kprintf("sizeof inode: %d\n",sizeof(cofs_inode_t));

	hd_queue_init();
	get_superblock(&superb);
	if(superb.magic != COFS_MAGIC) {
		panic("Invalid file system\n");
	}
	kprintf("size: %d, data: %d, num_inodes:%d, bitmap_start: %d, ino_start: %d\n", superb.size, superb.num_blocks, superb.num_inodes,
		superb.bitmap_start, superb.inode_start);
	// kprintf("Max file size: %d KB\n", MAX_FILE_SIZE * 512 / 1024);

	cofs_ncache = calloc(1, sizeof(cofs_ncache_t));
	cofs_ncache->list = list_open(NULL);

	int i, offs=0;

	minode_t *m;
	m = get_minode(1);
	milock(m);
	if(m->type != FS_DIRECTORY) {
		panic("root");
	}
	cofs_dirent_t dir;
	while((i = miread(m, (char *)&dir, offs, sizeof(dir))) > 0) {
		kprintf("---[%d\t%s\n", dir.inode, dir.name);
		if(!dir.inode) break;
		offs += i;
	}
	kprintf("---OK\n");
	miunlock(m);
	miput(m);
	char buf[4096];
	offs = 0;
	m = get_minode(6);
	milock(m);
	kprintf("size: %d\n", m->size);
	while((i = miread(m, buf, offs, 4096)) > 0) {
		kprintf(">%d %d\t", i, offs);
		offs += i;
	}
	miunlock(m);
	miput(m);
	kprintf("OK");
while(1);

	// while((i = miread(m, buf, offs, sizeof(buf)-1)) > 0) {
		// kprintf("read %d from %d\n", i, m->size);
		// offs += i;
	// }

	// miunlock(m);
	// miput(m);
	//while (1);
	return 0;
}
