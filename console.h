#ifndef _CONSOLE_H
#define _CONSOLE_H
#include <stdarg.h>
#include "int.h"
#include "vfs.h"
#include "ansi.h"

#define CIN_BUF_SIZE 256

struct cin {
    // Console input circular buffer
    struct {
        char buf[CIN_BUF_SIZE];
        int r;  // read pointer; downstream reads from here, until w
        int w;  // write pointer, begining of line, after last flash
        int e;  // edit pointer
        int t;  // top pointer
        int sz; // size of buffer
    } cb;
    // Console ANSI input status
    struct ansi_stat astat;
};

typedef int (*getc_t)();

/**
 * Write the string to stdout and halt kernel
 */
void panic(char *str, ...);

/**
 * Kernel low level, unbuffered printf
 */
void kprintf(char *fmt, ...);

void hexdump(void *buf, int len);
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
 * Initializes the console
 */
//void console_init(fs_node_t *cons);

void console_open(fs_node_t *node, uint32_t flags);
void console_in(getc_t getc);

#endif
