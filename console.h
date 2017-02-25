#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "vfs.h"

#define VID_ADDR	0xB8000

void scroll_line_up();
void scroll_line_down();
void scroll_page_up();
void scroll_page_down();

void panic(char * str, ...);
void kprintf(char *fmt, ...);
void console_init();
unsigned int console_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);
unsigned int console_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer);

void clrscr();

#endif
