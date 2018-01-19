/**
 * Yeah, i know, i could wrote these into ASM, but it is more clean this way
 */
#ifndef _X86_H
#define _X86_H

#include <sys/types.h>

/**
 * Reads count words from physical port into buf
 */
int port_read(uint16_t port, void *buf, int count);

/**
 * Writes count words from buf to physical port
 */
int port_write(uint16_t port, void *buf, int count);

#endif
