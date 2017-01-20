#include "delay.h"
#include "timer.h"

static unsigned long delay_cnt = 1;

static void __delay(unsigned long loops) {
	unsigned long i;
	for(i=0; i<loops; i++);
}

void delay(unsigned long miliseconds) {
	__delay(miliseconds * delay_cnt);
}

unsigned long calibrate_delay_loop(void){
	unsigned long prev_tick;
	unsigned int diff;
	// 1 - brute aproximation -> how many delay loops until one tick pass
	do {
		delay_cnt *= 2;
		prev_tick = timer_ticks;
		while(prev_tick == timer_ticks);	// wait for the next tick
		prev_tick = timer_ticks;
		__delay(delay_cnt);
	}
	while (prev_tick == timer_ticks);

	diff = delay_cnt;
	// 2 - fine aproximation by adding/deleting half of diff each loop
	do {
		diff /= 2;
		prev_tick = timer_ticks;
		while(prev_tick == timer_ticks);
		prev_tick = timer_ticks;
		__delay(delay_cnt);
		if(prev_tick == timer_ticks) {
			delay_cnt += diff;
		} else {
			delay_cnt -= diff;
		}
	} while(diff >= MS_PER_TICK);

	delay_cnt /= MS_PER_TICK; // 10 ms per tick;

	return delay_cnt;
}
