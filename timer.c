#include "x86.h"
#include "isr.h"
#include "irq.h"
#include "console.h"
#include "task.h"
#include "timer.h"

// programmable interval timer //
// timer has clock @ 1193180Hz
#define pit_data1 0x40		// data channel 1 register - generate irq0
#define pit_data2 0x41		// data channel 2 register - sys specific
#define pit_data3 0x42		// data channel 3 register - speaker
#define pit_cmmnd 0x43		// command register


void timer_phase (int hz) {
	int divisor = 1193180 / hz;		// compute divisor //
	outb(pit_cmmnd, 0x36);
	outb(pit_data1, divisor & 0xFF); // set low byte of divisor
	outb(pit_data1, divisor >> 8);	// high byte of divisor
}

void timer_handler(struct iregs *r) {
	timer_ticks++;
	// if(timer_ticks % 1000 == 0) {
		task_switch(r);
	// }
//	if(timer_ticks % 100 == 0) {
//		kprintf(".");
//	}
}


void timer_install(void) {
	timer_ticks = 0;
	timer_phase(100); // o intrerupere la a suta parte din secunda
	irq_install_handler(0, timer_handler); // seteaza timerul pe intreruperea 0
}


void timer_wait(int ms)
{
	ms = ms / 10;
	unsigned long eticks;
	eticks = timer_ticks + ms;
	while(timer_ticks < eticks) {
		nop();
	}
}
