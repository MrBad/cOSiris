//
//	Hard disk low level functions - PIO mode
//
#include <sys/types.h>
#include <stdlib.h>
#include "assert.h"
#include "hd.h"
#include "hd_queue.h"
#include "console.h"
#include "x86.h"
#include "irq.h"
#include "list.h"
#include "task.h"
#include "serial.h"

#define ATA_ERR 0x01
#define ATA_DRQ 0x08
#define ATA_DF	0x20	// drive fault
#define ATA_RDY 0x40
#define ATA_BSY 0x80

#define ATA0_BASE_IO 0x1F0
#define ATA0_BASE2_IO 0x3f0
#define ATA0_IRQ	14

#define ATA1_BASE_IO 0x170
#define ATA1_BASE2_IO 0x370
#define ATA1_IRQ	15

#define ATA_DRVSEL	6
#define ATA_STATUS	7

////


static int disk = 0;
static int base_port = ATA0_BASE_IO;
static int base2_port = ATA0_BASE2_IO;

#if 0
typedef struct cmd {
	hd_buf_t * hd_buf;
	task_t * task;
} cmd_t;

typedef struct cmd_queue {
	list_t *list;
	spin_lock_t lock;
} cmd_queue_t;

cmd_queue_t * cmd_queue;

cmd_t *new_cmd(hd_buf_t *hd_buf, task_t *task) {
	cmd_t *cmd = malloc(sizeof(*cmd));
	cmd->hd_buf = hd_buf;
	cmd->task = current_task;
	return cmd;
}
#endif
void free_cmd(void *data) {
	free(data);
}

int hd_wait_ready(bool check_error)
{
	int s;
	while(((s = inb(base_port|ATA_STATUS)) & (ATA_RDY|ATA_BSY)) != ATA_RDY); // waits until ata ready
	if(check_error && (s & (ATA_DF|ATA_ERR)) != 0) // if error... //
		return -1;
	return 0;
}

#if 0
void hd_handler()
{
	// kprintf("in hd handler\n");
	// switch_locked = true;
	KASSERT(cmd_queue->lock == 1);
	if(cmd_queue->list->num_items == 0) {
		return;
	}
	cmd_t *cmd = (cmd_t *) cmd_queue->list->head->data;
	KASSERT(cmd->hd_buf->lock == 1);
	if(!cmd->hd_buf->is_dirty && hd_wait_ready(true) >= 0) {
		port_read(base_port | 0, cmd->hd_buf->buf, 256);
	}
	list_del(cmd_queue->list, cmd_queue->list->head);
	wakeup(cmd->hd_buf);
	KASSERT(cmd_queue->list->num_items == 0);
	// kprintf("%d operations in q\n", cmd_queue->list->num_items);
	// switch_locked = false;

}

// void hd_start(unsigned int sector)
void hd_start(hd_buf_t *hdb)
{
	// kprintf("pid: %d invoking hd_start for block %d, %s\n",current_task->pid, hdb->block_no, hdb->is_dirty?"writing":"reading");
	int sector = hdb->block_no; // for now
	hd_wait_ready(0);
	outb(base2_port|6, 0x08); // generate interrupt when have data //
	outb(base_port|2, 1);
	outb(base_port|3, sector & 0xFF);
	outb(base_port|4, (sector >> 8) & 0xFF);
	outb(base_port|5, (sector >> 16) & 0xFF);
	outb(base_port|6, 0xE0|((disk % 2) << 4) | ((sector>>24)&0x0F));
	if(hdb->is_dirty) {
		// kprintf("writing block: %d", sector);
		outb(base_port|7, 0x30); // 0x30 - write sector //
		port_write(base_port|0, hdb->buf, 256); // no need to sleep on write //
	} else {
		// kprintf("reading block\n");
		outb(base_port|7, 0x20); // 0x20 - Command read sector //
		sleep_on(hdb);
	}
}

void hd_rw(hd_buf_t *hdb)
{
	KASSERT(hdb->lock == 1);
	spin_lock(&cmd_queue->lock);
	list_add(cmd_queue->list, new_cmd(hdb, current_task));
	if(((cmd_t*)cmd_queue->list->head->data)->hd_buf == hdb) {
		hd_start(hdb);
	}
	spin_unlock(&cmd_queue->lock);
}
#endif



spin_lock_t hd_lock;
void hd_rw(hd_buf_t *hdb) {
	KASSERT(hdb->lock==1);
	spin_lock(&hd_lock);
	int sector = hdb->block_no;
	int ret = 0;
	hd_wait_ready(0);
	outb(0x1f2, 1);
	outb(0x1f3, sector & 0xFF);
	outb(0x1f4, (sector >> 8) & 0xFF);
	outb(0x1f5, (sector >> 16) & 0xFF);
	outb(0x1f6, 0xE0|((disk % 2) << 4) | ((sector>>24)&0x0F));
	if(hdb->is_dirty) {
		outb(0x1f7, 0x30);
		if((ret = hd_wait_ready(1)) < 0) {
			panic("hd_wait_ready, write\n");
		}
		port_write(0x1f0, hdb->buf, 256);
	} else {
		outb(0x1f7, 0x20);
		if((ret = hd_wait_ready(1)) < 0) {
			panic("hd_wait_ready, read\n");
		}
		port_read(0x1f0, hdb->buf, 256);
	}
	spin_unlock(&hd_lock);
}


static bool check_disk(int disk_num)
{
	int i;
	bool found = false;
	if(disk_num > 3) return false;

	base_port = disk_num < 2 ? ATA0_BASE_IO : ATA1_BASE_IO;
	base2_port = disk_num < 2 ? ATA0_BASE2_IO : ATA1_BASE2_IO;
	disk = disk_num;
	outb(base_port|ATA_DRVSEL,  0xE0|((disk_num % 2) << 4) ); // check master on primary bus
	for(i = 0; i < 500; i++) {
		if(inb(base_port|ATA_STATUS) != 0) {
			kprintf("Found disk [hd%c]\n", disk_num+'a');
			found = true;
			break;
		}
	}
	return found;
}

void hd_init()
{
	int i;
	bool found;
	kprintf("hd_init()\n");
	found = false;
	for(i = 0; i < 4; i++) {
		if(check_disk(i)) {
			disk = i; found = true;
			break;
		}
	}
	if(!found) {
		panic("Cannot find any disk\n");
	}
	// irq_install_handler(14, hd_handler);
#if 0
	cmd_queue = calloc(1, sizeof(*cmd_queue));
	cmd_queue->list = list_open(free_cmd);
#endif
}
