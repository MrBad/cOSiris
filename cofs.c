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

struct cofs_superblock superb;

void get_superblock(cofs_superblock_t *sb)
{
	hd_buf_t *hdb = get_hd_buf(1);
	memcpy(sb, hdb->buf, sizeof(struct cofs_superblock));
	put_hd_buf(hdb);
}

void block_zero(uint32_t block) {

}

uint32_t block_alloc()
{
	hd_buf_t *hdb;
	int block, blk_idx, mask;
	for (block=0; block < superb.size; block += BITS_PER_BLOCK) {
		hdb = get_hd_buf(BITMAP_BLOCK(block, superb));
		for(blk_idx=0; blk_idx < BITS_PER_BLOCK && blk_idx < superb.size; blk_idx++) {
			mask = 1 << (blk_idx % 8);
			if((hdb->buf[blk_idx/8] & mask) == 0) {
				hdb->buf[blk_idx/8] |= mask;
				put_hd_buf(hdb);
				block_zero(block+blk_idx);
				return block+blk_idx;
			}
		}
		put_hd_buf(hdb);
	}
	panic("block_alloc() out of blocks");
	return 0;
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
	kprintf("Max file size: %d KB\n", MAX_FILE_SIZE * 512 / 1024);
	kprintf("blk: %d\n", block_alloc());
	kprintf("blk: %d\n", block_alloc());

	return 0;
}
