#ifndef _HD_QUEUE_H
#define _HD_QUEUE_H
#include "x86.h"	// spin_lock


#define BLOCK_SIZE 512


// disk buffer //
typedef struct hd_buf {
	int block_no;			// disk block number it represents //
	bool is_dirty;			// was this buffer written into it? //
	int ref_count;			// how many pointers to this buf - if 0 - free //
	spin_lock_t lock;		// use it in locking the buffer
	char buf[BLOCK_SIZE];	// the data in this block
} hd_buf_t;


int hd_queue_init();
hd_buf_t *get_hd_buf(int block_no);
void put_hd_buf(hd_buf_t *hdb);


#endif
