#ifndef _IRQ_H
#define _IRQ_H

#include <sys/types.h>
#include "isr.h"

typedef void (*irq_handler_t) (struct iregs *r);

/**
 * Setup hardware interrupts
 */
void irq_install();

/**
 * Installs a handler function, to be called on hardware interrupt irq
 */
void irq_install_handler(uint8_t irq, irq_handler_t handler);

/**
 * Uninstalls the handler function for interrupt irq
 */
void irq_uninstall_handler(uint8_t irq);

#endif
