// hard drive queued commands //
#include <sys/types.h>
#include <stdlib.h>		// malloc
#include "task.h"
#include "x86.h"		// i/o
#include "console.h"	// panic, kprintf
#include "list.h"
#include "hd.h"
#include "hd_queue.h"

// maximum number of buffers that we should keep in memory //
#define MAX_HD_BUFS 100

// this will be the disk buffers queue //
struct buf_queue {
	list_t *list;
	spin_lock_t lock;
};

// buffer queue //
struct buf_queue * buf_queue;

// allocate a new buffer //
hd_buf_t *new_hd_buf()
{
	hd_buf_t *hdb;
	if(!(hdb = malloc(sizeof(*hdb)))) {
		panic("malloc");
	}
	hdb->block_no = hdb->is_dirty = hdb->ref_count = 0;
	return hdb;
}

// frees an allocated buffer
void free_hd_buf(void *data)
{
	hd_buf_t *hdb = (hd_buf_t *) data;
	if(hdb->ref_count != 0) {
		panic("block %d still in use\n", hdb->block_no);
	}
	free(hdb);
}

// gets the buffer of block_no //
hd_buf_t *get_hd_buf(int block_no)
{
	node_t *n = NULL;
	hd_buf_t * hdb = NULL;
	spin_lock(&buf_queue->lock);
	for(n = buf_queue->list->head; n; n = n->next) {
		if(((hd_buf_t *)n->data)->block_no == block_no) {
			hdb = (hd_buf_t *) n->data;
			break;
		}
	}
	spin_unlock(&buf_queue->lock);
	if(hdb) {
		hdb->ref_count++;
		spin_lock(&hdb->lock);
	}
	else {
		spin_lock(&buf_queue->lock);
		// create a new one //
		if(buf_queue->list->num_items < MAX_HD_BUFS) {
			hdb = new_hd_buf();
			list_add(buf_queue->list, hdb);
		}
		// not found, we are going to reuse one //
		else {
			for(n = buf_queue->list->head; n; n = n->next) {
				hdb = (hd_buf_t *) n->data;
				if(hdb->ref_count == 0 && !hdb->is_dirty && !hdb->lock) {
					break;
				}
			}
			if(!hdb) {
				panic("max_hd_bufs %d reached and cannot find any free buf!\n", MAX_HD_BUFS);
			}
		}
		hdb->lock = 0;
		hdb->block_no = block_no;
		hdb->is_dirty = false;
		hdb->ref_count = 1;
		spin_lock(&hdb->lock);

		spin_unlock(&buf_queue->lock);
		hd_rw(hdb);
	}
	return hdb;
}

void put_hd_buf(hd_buf_t *hdb)
{
	// if is dirty and no reference to it //
	// write it back to disk //
	spin_lock(&buf_queue->lock);
	if(hdb->ref_count <= 0) {
		panic("pid: %d trying to close not opened block %d\n", current_task->pid, hdb->block_no);
	}
	hdb->ref_count--;
	if(hdb->is_dirty && hdb->ref_count == 0) {
		hd_rw(hdb);
		hdb->is_dirty = 0;
	}
	spin_unlock(&hdb->lock);
	spin_unlock(&buf_queue->lock);
}

// init the queue
int hd_queue_init()
{
	hd_init();

	if(!(buf_queue = malloc(sizeof(struct buf_queue)))) {
		panic("malloc");
	}
	if(!(buf_queue->list = list_open(free_hd_buf))) {
		panic("list_open");
	}
	// kprintf("hd queue\n\n");
	//
	// node_t *n;
	// if(fork() == 0) { fork();
	// 	kprintf("child reading\n");
	// 	hd_buf_t *hdb = get_hd_buf(0);
	// 	uint16_t *b = (uint16_t *) hdb->buf;
	// 	hdb->is_dirty = true;
	// 	b[0]=0xdead;
	// 	put_hd_buf(hdb);
	//
	// 	hd_buf_t *hdb2 = get_hd_buf(2);
	// 	hd_buf_t *hdb3 = get_hd_buf(3);
	// 	put_hd_buf(hdb3);
	// 	put_hd_buf(hdb2);
	// 	// cli();
	// 	for(n=buf_queue->list->head; n; n=n->next) {
	// 		kprintf("pid:%d block %d in cache, dirty: %d, locked: %d\n", current_task->pid, ((hd_buf_t *)n->data)->block_no,((hd_buf_t *)n->data)->is_dirty, ((hd_buf_t *)n->data)->lock);
	// 	}
	// 	// sti();
	// 	task_exit(0);
	//
	// }
	// if(fork()==0) {
	// 	hd_buf_t *hdb5 = get_hd_buf(5);
	// 	hd_buf_t *hdb4 = get_hd_buf(4);
	// 	strcpy(hdb4->buf, "Vio is testing");
	// 	hdb4->is_dirty = true;
	// 	put_hd_buf(hdb4);
	// 	put_hd_buf(hdb5);
	// 	task_exit(0);
	// }

	return 0;
}
