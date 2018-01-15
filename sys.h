/**
 * Function from sys.asm
 */
#ifndef _SYS_H
#define _SYS_H

#include <sys/types.h>

void switch_to_user_mode_asm(uint32_t code_addr, uint32_t stack_hi_addr);

void tss_flush();

uint32_t call_sys(void *func, uint32_t p1, uint32_t p2, uint32_t p3, 
        uint32_t p4, uint32_t p5);

#endif
