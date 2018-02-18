/**
 * cOSiris *nix like file system
 */
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "assert.h"
#include "console.h"
#include "hd.h"
#include "hd_queue.h"
#include "cofs.h"
#include "list.h"
#include "vfs.h"
#include "bname.h"

#define MAX_CACHED_NODES 100
#define COFS_ROOT_INUM 1

struct cofs_superblock superb;

/**
 * A linked list inode cache type
 */
typedef struct {
    spin_lock_t lock;
    list_t *list;
} cofs_cache_t;

// cofs inode caching list
// TODO: move to a more advanced structure, like a binary search tree
cofs_cache_t *cofs_cache;

void cofs_open(fs_node_t *node, unsigned int flags);
void cofs_close(fs_node_t *node);
unsigned int cofs_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);
unsigned int cofs_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);
fs_node_t *cofs_finddir(fs_node_t *node, char *name);
struct dirent *cofs_readdir(fs_node_t *node, unsigned int index);
fs_node_t *cofs_mkdir(fs_node_t *node, char *name, unsigned int mode);
fs_node_t *cofs_creat(fs_node_t *node, char *name, unsigned int mode);
fs_node_t *cofs_mknod(fs_node_t *dir, char *name, uint32_t mode, uint32_t dev);
int cofs_truncate(fs_node_t *node, unsigned int length);
int cofs_link(fs_node_t *parent, fs_node_t *node, char *name);
int cofs_unlink(fs_node_t *node, char *name);
fs_node_t *cofs_get_node(uint32_t inum);

/**
 * Retrieves cofs superblock from disk
 */
void get_superblock(cofs_superblock_t *sb)
{
    hd_buf_t *hdb = get_hd_buf(1);
    memcpy(sb, hdb->buf, sizeof(struct cofs_superblock));
    put_hd_buf(hdb);
}

/**
 * Zeroes a disk block
 */
void block_zero(uint32_t block)
{
    hd_buf_t *hdb = get_hd_buf(block);
    hdb->is_dirty = true;
    memset(hdb->buf, 0, sizeof(hdb->buf));
    put_hd_buf(hdb);
}

/**
 * Finds first unallocated block
 */
uint32_t block_alloc()
{
    hd_buf_t *hdb;
    uint32_t block, scan, idx, mask;
    for (block = 0; block < superb.size; block += BITS_PER_BLOCK) {
        hdb = get_hd_buf(BITMAP_BLOCK(block, superb));
        // fast forward using big index until find a non full int //
        for (scan = 0; scan < BLOCK_SIZE / sizeof(uint32_t); scan++) {
            if (((uint32_t *) hdb->buf)[scan] != 0xFFFFFFFF) {
                break;
            }
        }
        if (scan != BLOCK_SIZE / sizeof(uint32_t)) {
            for (idx = scan * 32; idx < (scan + 1) * 32; idx++) {
                mask = 1 << (idx % 8);
                if ((hdb->buf[idx / 8] & mask) == 0) {
                    hdb->buf[idx / 8] |= mask;
                    hdb->is_dirty = true;
                    put_hd_buf(hdb);
                    block_zero(block + idx);
                    return block + idx;
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
    //kprintf("blk %d ", block);
    hd_buf_t *hdb;
    uint32_t bitmap_block, idx, mask;
    bitmap_block = BITMAP_BLOCK(block, superb);
    hdb = get_hd_buf(bitmap_block);
    idx = block % BITS_PER_BLOCK;
    mask = 1 << (idx % 8);
    if ((hdb->buf[idx / 8] & mask) == 0) {
        panic("Block %d allready free", block);
    }
    hdb->buf[idx / 8] &= ~mask;
    hdb->is_dirty = true;
    put_hd_buf(hdb);
}

/**
 * Allocates a free inode on disk
 */
fs_node_t *inode_alloc(unsigned short int type)
{
    unsigned int ino_num;
    hd_buf_t *hdb;
    cofs_inode_t *ino;
    for (ino_num = 1; ino_num < superb.num_inodes; ino_num++) {
        hdb = get_hd_buf(INO_BLK(ino_num, superb));
        ino = (cofs_inode_t *) hdb->buf + ino_num % NUM_INOPB;
        if (ino->type == 0) { // if type == 0 -> is unalocated
            memset(ino, 0, sizeof(*ino));
            ino->type = type & 0xF000;
            if (ino->type & FS_CHARDEVICE || ino->type & FS_BLOCKDEVICE) {
                ino->major = major(type);
                ino->minor = minor(type);
            }
            hdb->is_dirty = true;
            put_hd_buf(hdb);
            return cofs_get_node(ino_num);
        }
        put_hd_buf(hdb);
    }
    panic("No free inodes\n");
    return NULL;
}

/**
 * Update from memory inode to disk
 */
void cofs_update_node(fs_node_t *node)
{
    KASSERT(node);
    cofs_inode_t *ino;
    hd_buf_t *hdb = get_hd_buf(INO_BLK(node->inode, superb));
    ino = (cofs_inode_t *) hdb->buf + node->inode % NUM_INOPB;
    ino->type = (node->type & 0xF000) | node->mask;
    ino->major = node->major;
    ino->minor = node->minor;
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

/**
 * Finds the inode number inum and returns a cached fs_node.
 * Does not retrieve the inode from disk; lock does that
 */
fs_node_t *cofs_get_node(uint32_t inum)
{
    node_t *n;
    fs_node_t *node, *free_node = NULL;
    spin_lock(&cofs_cache->lock);
    for (n = cofs_cache->list->head; n; n = n->next) {
        node = (fs_node_t *) n->data;
        if (node->inode == inum) {
            node->ref_count++;
            spin_unlock(&cofs_cache->lock);
            return node;
        }
        if (node->ref_count == 0) { // unused by any process
            free_node = node;
        }
    }
    if (!free_node) {
        if (cofs_cache->list->num_items >= MAX_CACHED_NODES) {
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
    node->mknod = cofs_mknod;
    node->creat = cofs_creat;
    node->truncate = cofs_truncate;
    node->link = cofs_link;
    node->unlink = cofs_unlink;
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

/**
 * Locks the inode,
 * Loads it's disk data into it, if neccesary
 */
void cofs_lock(fs_node_t *node)
{
    hd_buf_t *hdb;

    if (node == NULL)
        panic("cofs_lock - no node\n");
    if (node->ref_count <= 0)
        panic("ref_count for node %s is %d\n", node->name, node->ref_count);

    spin_lock(&node->lock);
    if (node->type == 0) {
        // kprintf("geting from disk: %d\n", node->inode);
        hdb = get_hd_buf(INO_BLK(node->inode, superb));
        cofs_inode_t *ino = (cofs_inode_t *) hdb->buf + node->inode % NUM_INOPB;
        node->type = ino->type & FS_IFMT;
        node->mask = ino->type & ~FS_IFMT;
        node->major = ino->major;
        node->minor = ino->minor;
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
        if (node->type == 0) {
            panic("cofs_lock - node type is 0\n");
        }
    }
}

/**
 * Unlocks an inode
 */
void cofs_unlock(fs_node_t *node)
{
    if (node == NULL || node->lock != 1 || node->ref_count < 0) {
        kprintf("cofs_unlock()");
        if (node == NULL)
            kprintf("node is null\n");
        if (node->lock != 1)
            kprintf("node not locked\n");
        if (node->ref_count < 0)
            kprintf("ref_count < 0\n");
        panic("");
    }
    spin_unlock(&node->lock);
}

/**
 * Release the node.
 * If no num_links reference it, node will be truncated.
 */
void cofs_put_node(fs_node_t *node)
{
    spin_lock(&cofs_cache->lock);
    if (node->ref_count == 0) {
        panic("already put %s\n", node->name);
    }
    if (node->ref_count == 1 && node->type > 0 && node->num_links == 0) {
        kprintf("Unlinking %s\n", node->name);
        spin_unlock(&cofs_cache->lock);
        cofs_truncate(node, 0);
        node->type = 0;
        cofs_update_node(node);
        spin_lock(&cofs_cache->lock);
    }
    if (node->inode == 1) {
//		kprintf("put root, count: %d\n", node->ref_count);
        if (node->ref_count > 1)
            node->ref_count--;
    } else
        node->ref_count--;
    spin_unlock(&cofs_cache->lock);
}

//
//	 fbn - file block number 0, 1, 2...
//	return disk block address of nth block in inode ino
//	if there is no such block, it allocates one
//
uint32_t block_map(fs_node_t *node, uint32_t fbn)
{
    hd_buf_t *hdb;
    uint32_t addr;

    if (fbn < NUM_DIRECT) {
        if (node->addrs[fbn] == 0) {
            node->addrs[fbn] = block_alloc();
        }
        return node->addrs[fbn];
    }
    else if (fbn < NUM_DIRECT + NUM_SIND) {
        // kprintf("indirect\n");
        if (node->addrs[SIND_IDX] == 0) {
            node->addrs[SIND_IDX] = block_alloc();
        }
        hdb = get_hd_buf(node->addrs[SIND_IDX]);
        if (((uint32_t *) hdb->buf)[fbn - NUM_DIRECT] == 0) {
            ((uint32_t *) hdb->buf)[fbn - NUM_DIRECT] = block_alloc();
            hdb->is_dirty = true;
        }
        addr = ((uint32_t *) hdb->buf)[fbn - NUM_DIRECT];
        put_hd_buf(hdb);
        return addr;
    }
    else if (fbn < NUM_DIRECT + NUM_SIND + NUM_DIND) {
        // kprintf("double indirect\n");
        uint32_t midx = (fbn - NUM_DIRECT - NUM_SIND) / NUM_SIND;
        uint32_t sidx = (fbn - NUM_DIRECT - NUM_SIND) % NUM_SIND;
        if (node->addrs[DIND_IDX] == 0) {
            node->addrs[DIND_IDX] = block_alloc();
        }
        hdb = get_hd_buf(node->addrs[DIND_IDX]);
        if (((uint32_t *) hdb->buf)[midx] == 0) {
            ((uint32_t *) hdb->buf)[midx] = block_alloc();
            hdb->is_dirty = true;
        }
        uint32_t mblk = ((uint32_t *) hdb->buf)[midx];
        put_hd_buf(hdb);
        hdb = get_hd_buf(mblk);
        if (((uint32_t *) hdb->buf)[sidx] == 0) {
            ((uint32_t *) hdb->buf)[sidx] = block_alloc();
            hdb->is_dirty = true;
        }
        addr = ((uint32_t *) hdb->buf)[sidx];
        put_hd_buf(hdb);
        return addr;
    }
    panic("block_map out of range\n");
    return 0;
}

int scan_block(unsigned int blockno)
{
    hd_buf_t *hdb;
    unsigned int scan;
    hdb = get_hd_buf(blockno);
    int i = 0;
    for (scan = 0; scan < BLOCK_SIZE / sizeof(uint32_t); scan++) {
        if (((uint32_t *) hdb->buf)[scan] != 0) {
            i++;
        }
    }
    put_hd_buf(hdb);
    return i;
}

/**
 * Truncates the node
 */
int cofs_truncate(fs_node_t *node, unsigned int length)
{
///	kprintf("in truncate\n");
    unsigned int fbn, fbs, fbe; // file block num, start, end //
    hd_buf_t *hdb;
    uint32_t addr;
    KASSERT(length <= node->size);
    KASSERT(length % BLOCK_SIZE == 0);
    fbs = (length / BLOCK_SIZE);
    fbe = (node->size / BLOCK_SIZE) + 1;
//	kprintf("fbs:%d, fbe: %d\n", fbs, fbe);
    int i = 0;
    int j = 0;

    for (fbn = fbs; fbn < fbe; fbn++) {
        if (fbn < NUM_DIRECT) {
            if (node->addrs[fbn]) {
                block_free(node->addrs[fbn]);
                j++;
                node->addrs[fbn] = 0;
            }
        } else if (fbn < NUM_DIRECT + NUM_SIND) {
            hdb = get_hd_buf(node->addrs[SIND_IDX]);
            addr = ((uint32_t *) hdb->buf)[fbn - NUM_DIRECT];
            ((uint32_t *) hdb->buf)[fbn - NUM_DIRECT] = 0;
            hdb->is_dirty = 1;
            block_free(addr);
            j++;
            put_hd_buf(hdb);
            if (scan_block(node->addrs[SIND_IDX]) == 0) {
                block_free(node->addrs[SIND_IDX]);
                i++;
                node->addrs[SIND_IDX] = 0;
            }
        } else if (fbn < NUM_DIRECT + NUM_SIND + NUM_DIND) {
            uint32_t midx = (fbn - NUM_DIRECT - NUM_SIND) / NUM_SIND;
            uint32_t sidx = (fbn - NUM_DIRECT - NUM_SIND) % NUM_SIND;

            hdb = get_hd_buf(node->addrs[DIND_IDX]);
            uint32_t mblk = ((uint32_t *) hdb->buf)[midx];
            put_hd_buf(hdb);

            hdb = get_hd_buf(mblk);
            addr = ((uint32_t *) hdb->buf)[sidx];
            ((uint32_t *) hdb->buf)[sidx] = 0;
            hdb->is_dirty = true;
            block_free(addr);
            j++;
            put_hd_buf(hdb);
            // to be tested //
            int x;
            if ((x = scan_block(mblk)) == 0) {
                block_free(mblk);
                i++;
                hdb = get_hd_buf(node->addrs[DIND_IDX]);
                ((uint32_t *) hdb->buf)[midx] = 0;
                hdb->is_dirty = 1;
                put_hd_buf(hdb);
            }
            if (scan_block(node->addrs[DIND_IDX]) == 0) {
                block_free(node->addrs[DIND_IDX]);
                i++;
                node->addrs[DIND_IDX] = 0;
            }
        }
    }
//	kprintf("free - %d direct blocks and %d indirect, total: %d\n", j,i, j+i);
    node->size = length;
    cofs_update_node(node);
    return length;
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/**
 * Read max size bytes from node offset into buffer
 */
unsigned int cofs_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
    hd_buf_t *hdb;
    if (offset > node->size) {
        return 0;
    }
    if (offset + size > node->size) {
        size = node->size - offset;
    }
    unsigned int total, num_bytes;
    for (total = 0; total < size; total += num_bytes) {
        hdb = get_hd_buf(block_map(node, offset / BLOCK_SIZE));
        num_bytes = min(size - total, BLOCK_SIZE - offset % BLOCK_SIZE);
        memcpy(buffer, hdb->buf + offset % BLOCK_SIZE, num_bytes);
        offset += num_bytes;
        buffer += num_bytes;
        put_hd_buf(hdb);
    }
    return size;
}

/**
 * Writes size bytes from buffer, at offset in node
 */
unsigned int cofs_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
    hd_buf_t *hdb;
    unsigned int total, num_bytes = 0;

    if (offset > node->size) {
        return 0;
    }
    if (offset + size > MAX_FILE_SIZE * BLOCK_SIZE) {
        return 0;
    }
    for (total = 0; total < size; total += num_bytes) {
        hdb = get_hd_buf(block_map(node, offset / BLOCK_SIZE));
        num_bytes = min(size - total, BLOCK_SIZE - offset % BLOCK_SIZE);
        memcpy(hdb->buf + offset % BLOCK_SIZE, buffer, num_bytes);
        offset += num_bytes;
        buffer += num_bytes;
        hdb->is_dirty = true;
        put_hd_buf(hdb);
    }
    // kprintf("write %s, offs: %d, size:%d, node_size: %d\n", node->name, offset, size, node->size);
    if (size > 0 && offset > node->size) {
        // kprintf("update node\n");
        node->size = offset;
        cofs_update_node(node);
    }
    return size;
}

/**
 * Opens the node. TODO handle flags
 */
void cofs_open(fs_node_t *node, unsigned int flags)
{
    (void) flags;
    kprintf("cofs_open: %s\n", node->name);
    cofs_lock(node); // should we lock only if writing?!
}

/**
 * Close the node
 */
void cofs_close(fs_node_t *node)
{
    if (node->lock)
        cofs_unlock(node);
    cofs_put_node(node);
}

/**
 * Finds and retrieve node having name, inside parent node directory
 * increment ref_count, to keep it's reference in memory
 * user should put_node after retrieving it
 */
fs_node_t *cofs_finddir(fs_node_t *node, char *name)
{
    if (!(node->type & FS_DIRECTORY)) {
        panic("cofs_finddir() In search for %s - node: %s is not a directory\n", name, node->name);
    }

    cofs_dirent_t dir;
    int n, offs = 0, inum = 0;
    fs_node_t *new_node;

    while ((n = cofs_read(node, offs, sizeof(dir), (char *) &dir)) > 0) {
        if (strcmp(dir.name, name) == 0) {
            inum = dir.inode;
            break;
        }
        offs += n;
    }
    if (inum == 0) {
        return NULL;
    }
    new_node = cofs_get_node(inum);
    cofs_lock(new_node);
    if (new_node->inode != 1)
        strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    cofs_unlock(new_node);
    // cofs_put_node(new_node);
    return new_node;
}

struct dirent dirent;

struct dirent *cofs_readdir(fs_node_t *node, unsigned int index)
{
    unsigned int n, offs;
    cofs_dirent_t dir;
    KASSERT(node->lock == 0);
    cofs_lock(node);
    memset(&dirent, 0, sizeof(dirent));
    offs = index * sizeof(cofs_dirent_t);
    if ((n = cofs_read(node, offs, sizeof(dir), (char *) &dir)) > 0) {
        if (dir.inode == 0) {
            index++;
            cofs_unlock(node);
            return &dirent;
        }
        strncpy(dirent.d_name, dir.name, sizeof(dirent.d_name) - 1);
        dirent.d_ino = dir.inode;
    } else {
        cofs_unlock(node);
        return NULL;
    }
    cofs_unlock(node);
    return &dirent;
}

/**
 * Inserts name and inode into parent directory node
 */
int cofs_dirlink(fs_node_t *node, char *name, unsigned int inum)
{
    fs_node_t *n;
    if ((n = cofs_finddir(node, name)) != NULL) {
        kprintf("cofs_dirlink - file %s exists\n", name);
        cofs_put_node(n);
        return -1;
    }
    int num_bytes;
    cofs_dirent_t dir = {0};
    unsigned int offs = 0;
    while ((num_bytes = cofs_read(node, offs, sizeof(dir), (char *) &dir)) > 0) {
        if (dir.inode == 0) { // recycle old node
            break;
        }
        offs += num_bytes;
    }
    strncpy(dir.name, name, sizeof(dir.name));
    dir.inode = inum;
    if ((cofs_write(node, offs, sizeof(dir), (char *) &dir)) != sizeof(dir)) {
        panic("cofs_dirlink");
    }
    return 0;
}

/**
 * Hard linking a node into a parent directory
 */
int cofs_link(fs_node_t *parent, fs_node_t *node, char *name)
{
    int ret;
    cofs_lock(node);
    node->num_links++;
    cofs_update_node(node);
    cofs_lock(parent);
    ret = cofs_dirlink(parent, name, node->inode);
    cofs_unlock(parent);
    cofs_unlock(node);
    //kprintf("node reco: %d, parent refco: %d, num_links: %d\n",
    // node->ref_count, parent->ref_count, node->num_links);
    return ret;
}

/**
 * Creates a directory in parent node; mkdir; increments ref_count
 */
fs_node_t *cofs_mkdir(fs_node_t *node, char *name, unsigned int mode)
{
    fs_node_t *new_dir;
    if ((new_dir = cofs_finddir(node, name)) != NULL) {
        cofs_put_node(new_dir);
        kprintf("directory %s exists\n", name);
        return NULL;
    }
    new_dir = inode_alloc(FS_DIRECTORY);
    cofs_lock(new_dir);
    strncpy(new_dir->name, name, sizeof(new_dir->name) - 1);
    new_dir->mask = mode; // and mask?! //
    new_dir->num_links = 1;
    // create . and .. //
    cofs_dirlink(new_dir, ".", new_dir->inode);
    cofs_dirlink(new_dir, "..", node->inode);
    cofs_update_node(new_dir);
    cofs_unlock(new_dir);

    cofs_lock(node);
    cofs_dirlink(node, name, new_dir->inode); // link it to parent
    cofs_unlock(node);

    return new_dir;
}

fs_node_t *cofs_mknod(fs_node_t *dir, char *name, uint32_t mode, uint32_t dev)
{
    fs_node_t *node;
    if ((node = cofs_finddir(dir, name))) {
        cofs_put_node(node);
        kprintf("file %s exists\n", name);
        return NULL;
    }
    node = inode_alloc((dev & FS_IFMT) | (mode & 0xFF));
    cofs_lock(node);
    strncpy(node->name, name, sizeof(node->name) - 1);
    dev &= ~FS_IFMT;
    node->major = major(dev);
    node->minor = minor(dev);
    node->mask = mode;
    node->num_links = 1;
    cofs_update_node(node);
    cofs_unlock(node);

    cofs_lock(dir);
    cofs_dirlink(dir, name, node->inode);
    cofs_unlock(dir);

    return node;
}

/**
 * Creates a new node inside parent node
 */
fs_node_t *cofs_creat(fs_node_t *node, char *name, unsigned int mode)
{
    KASSERT(node->type & FS_DIRECTORY);
    fs_node_t *new_file;
    if ((new_file = cofs_finddir(node, name)) != NULL) {
        cofs_put_node(new_file);
        kprintf("File exists\n");
        return NULL;
    }
    new_file = inode_alloc(FS_FILE);
    cofs_lock(new_file);
    strncpy(new_file->name, name, sizeof(new_file->name) - 1);
    new_file->mask = mode; // and mask? //
    new_file->num_links = 1;
    cofs_update_node(new_file);
    cofs_unlock(new_file);
    cofs_lock(node);
    cofs_dirlink(node, name, new_file->inode);
    cofs_unlock(node);
    return new_file;
}

/**
 * Deletes the node with name from parent
 */
int cofs_unlink(fs_node_t *parent, char *name)
{
    KASSERT(parent);
    fs_node_t *node;
    int n, found = 0;
    cofs_dirent_t dir;
    unsigned int offs = 0;

    if (!(node = cofs_finddir(parent, name)))
        return -1;
    cofs_lock(node);
    cofs_lock(parent);
    while ((n = cofs_read(parent, offs, sizeof(dir), (char *) &dir)) > 0) {
        if (strcmp(dir.name, node->name) == 0) {
            found = true;
            break;
        }
        offs += n;
    }
    KASSERT(found);
    dir.inode = 0;
    memset(dir.name, 0, sizeof(dir.name));
    if ((cofs_write(parent, offs, sizeof(dir), (char *) &dir)) != sizeof(dir)) {
        panic("cofs_unlink");
    }
    cofs_update_node(parent);
    cofs_unlock(parent);

    //kprintf("num lnk: %d\n", node->num_links);
    node->num_links--; // and put will truncate it if 0 //
    cofs_update_node(node);
    cofs_unlock(node);
    cofs_put_node(node);
    cofs_put_node(parent);
    return 0;
}

/**
 * Dumps internal cofs inode cache
 */
void cofs_dump_cache()
{
    node_t *n;
    fs_node_t *node;
    spin_lock(&cofs_cache->lock);
    kprintf("%d nodes in cache\n", cofs_cache->list->num_items);
    for (n = cofs_cache->list->head; n; n = n->next) {
        node = (fs_node_t *) n->data;
        kprintf("name: %s, ino: %d, ref_count: %d, %s\n",
                node->name, node->inode, node->ref_count,
                node->lock ? "locked" : "unlocked");
    }
    spin_unlock(&cofs_cache->lock);
}

/**
 * cofs initialization.
 * Returns root fs node
 */
fs_node_t *cofs_init()
{
    fs_node_t *root;

    KASSERT(BLOCK_SIZE % sizeof(struct cofs_inode) == 0);
    KASSERT(BLOCK_SIZE % sizeof(cofs_dirent_t) == 0);

    hd_queue_init();

    get_superblock(&superb);
    if (superb.magic != COFS_MAGIC) {
        panic("Invalid file system, cannot find COFS_MAGIC, got: %u\n",
              superb.magic);
    }

    kprintf("Loaded COFS superblock:\n"
                    " size: %u blocks, %luMB\n"
                    " data: %u blocks\n"
                    " num_inodes:%u\n"
                    " bitmap_start: %u\n"
                    " ino_start: %u\n"
                    " data_block: %u\n",
            superb.size, superb.size * BLOCK_SIZE / 1024 / 1024,
            superb.num_blocks, superb.num_inodes,
            superb.bitmap_start, superb.inode_start, superb.data_block);

    kprintf("Max file size: %d KB\n", MAX_FILE_SIZE * 512 / 1024);

    cofs_cache = calloc(1, sizeof(*cofs_cache));
    cofs_cache->list = list_open(NULL);

    root = cofs_get_node(COFS_ROOT_INUM);
    cofs_lock(root);
    if (root->type != FS_DIRECTORY) {
        panic("root is not a directory");
    }
    strcpy(root->name, "/");
    cofs_unlock(root);

    return root;
}
