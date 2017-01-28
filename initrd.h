#ifndef _INITRD_H
#define _INITRD_H
#include "vfs.h"

fs_node_t *initrd_init(unsigned int location);
void initrd_add_node(struct fs_node *node);
struct fs_node * initrd_new_node();
void dump_fs();
#endif
