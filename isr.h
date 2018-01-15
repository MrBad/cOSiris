#ifndef _ISR_H
#define _ISR_H

#include <sys/types.h>

struct iregs {
    uint32_t gs, fs, es, ds;      // pushed the segs last
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pushed by pusha
    uint32_t int_no, err_code;    // our push byte and error codes do this
    uint32_t eip, cs, eflags, useresp, ss; // pushed by the processor
};

typedef uint32_t (*isr_handler_t)(struct iregs *r);

/**
 * Initialize the interrupts
 */
void isr_init();

/**
 * Installs a handler function to be called for a specific interrupt isr_num
 */
void isr_install_handler(uint8_t isr_num, isr_handler_t);

/**
 * Uninstalls a handler function for a specific interrupt
 */
void isr_uninstall_handler(uint8_t isr_num);

#endif
