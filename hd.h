#ifndef _HD_H
#define _HD_H

#include "hd_queue.h"

/**
 * Hard drive read/write of a block.
 *  if block is dirty, it will be written to disk, otherwise read.
 */
void hd_rw(hd_buf_t *hdb);
void hd_init();

#endif