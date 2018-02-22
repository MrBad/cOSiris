#ifndef _ISR_H
#define _ISR_H

#include <sys/types.h>

struct iregs {
    uint32_t gs;        // 0
    uint32_t fs;        // 4
    uint32_t es;        // 8
    uint32_t ds;        // 12
    uint32_t edi;       // 16
    uint32_t esi;       // 20
    uint32_t ebp;       // 24
    uint32_t esp;       // 28
    uint32_t ebx;       // 32
    uint32_t edx;       // 36
    uint32_t ecx;       // 40
    uint32_t eax;       // 44
    uint32_t int_no;    // 48
    uint32_t err_code;  // 52
    uint32_t eip;       // 56
    uint32_t cs;        // 60
    uint32_t eflags;    // 64
    uint32_t useresp;   // 68
    uint32_t ss;        // 72
};

typedef struct iregs iregs_t;

typedef uint32_t (*int_handler_t) (struct iregs *r);

void *int_routines[256];

/**
 * Initialize the interrupts
 */
void int_init();

/**
 * Installs a handler function to be called for a specific interrupt isr_num
 */
void int_install_handler(uint8_t int_no, int_handler_t);

/**
 * Uninstalls a handler function for a specific interrupt
 */
void int_uninstall_handler(uint8_t int_no);

#endif
