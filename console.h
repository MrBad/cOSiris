#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "isr.h"
#include "vfs.h"

#define VID_ADDR    0xB8000

void scroll_line_up();
void scroll_line_down();
void scroll_page_up();
void scroll_page_down();

/**
 * When you have last words to say
 */
void panic(char * str, ...);

/**
 * Like printf, but for kernel
 */
void kprintf(char *fmt, ...);

/**
 * Write
 */
unsigned int console_write(fs_node_t *node, unsigned int offset,
                           unsigned int size, char *buffer);

/**
 * Read
 */
unsigned int console_read(fs_node_t *node, unsigned int offset,
                          unsigned int size, char *buffer);

/**
 * Clears the screen
 */
void clrscr();

/**
 * Console interrupt handler
 */
void console_handler(struct iregs *r);

/**
 * Initializes the console
 */
void console_init();

#endif
