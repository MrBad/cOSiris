/**
 * Simple (naive) delay implementation
 * Not to be used when multitasking is ON, because it will be interrupted
 * Probably i will rewrite it to use PIC
 */
#ifndef _DELAY_H
#define _DELAY_H

/**
 * Delay ms
 */
void delay(unsigned long milliseconds);

/**
 * Calibrate the delay loop
 */
unsigned long calibrate_delay_loop(void);

#endif
