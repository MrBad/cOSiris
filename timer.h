#ifndef _TIMER_H
#define _TIMER_H

// timer will tick ervery 10 milliseconds //
#define MS_PER_TICK 10

// total number of ticks since started //
unsigned int timer_ticks;

/**
 * Installs the timer
 */
void timer_install();

#endif
