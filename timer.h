#ifndef _TIMER_H
#define _TIMER_H

#define MS_PER_TICK 10

unsigned int timer_ticks;

void timer_install(void);
void timer_wait(int ms);

#endif
