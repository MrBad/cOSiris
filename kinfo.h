#ifndef _KINFO_H
#define _KINFO_H

#include <sys/types.h>
#include "multiboot.h"

/**
 * A structure to describe memory blocks available in the system
 */
struct minfo {
    uint64_t base;      // Where the memory block starts, in bytes
    uint64_t length;    // How long it is, bytes
    uint8_t  type;      // Type of memory
};

/**
 * Kernel infos passed by linker and get with get_kernel_info in asm
 */
struct kinfo {
	uint32_t code;	// address where kernel starts - 1M
	uint32_t start;	// where asm "start" routine is -> kernel entry point
	uint32_t data;	// where the data begin
	uint32_t bss;	// where global vars are
	uint32_t end;	// where kernel ends
	uint32_t size;	// kernel size == end - code
	uint32_t stack;	// where stack starts (low); starts @ stack+stack_size
	uint32_t stack_size; // stack size
	struct minfo minfo[32]; // we will keep track of memory blocks here
};

/**
 * Fills kinfo structure, using multiboot values magic and mb
 * Also fills code - stack_size values, by calling ASM
 */
void get_kernel_info(uint32_t magic, multiboot_header *mb, struct kinfo *kinfo);

#endif
