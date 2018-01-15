#include "delay.h"
#include "timer.h"

// how many empty loops are in a ms. in this CPU ?
static unsigned long delay_cnt = 1;

static void __delay(unsigned long loops)
{
    unsigned long i;
    for (i = 0; i < loops; i++);
}

/**
 * Do a delay
 */
void delay(unsigned long milliseconds)
{
    __delay(milliseconds * delay_cnt);
}

/**
 * Calibrating the delay loop, to know how many empty loops in a ms they are
 */
unsigned long calibrate_delay_loop(void)
{
    unsigned long prev_tick;
    unsigned int diff;
    // 1'st - brute approximation -> how many delay loops until one tick pass
    do {
        delay_cnt *= 2;
        prev_tick = timer_ticks;
        while (prev_tick == timer_ticks) // wait for the next tick
            ;
        prev_tick = timer_ticks;
        __delay(delay_cnt);
    }
    while (prev_tick == timer_ticks);

    diff = delay_cnt;
    // 2'nd - fine approximation by adding/deleting half of diff each loop
    do {
        diff /= 2;
        prev_tick = timer_ticks;
        while (prev_tick == timer_ticks);
        prev_tick = timer_ticks;
        __delay(delay_cnt);
        if (prev_tick == timer_ticks) {
            delay_cnt += diff;
        } else {
            delay_cnt -= diff;
        }
    } while (diff >= MS_PER_TICK);

    delay_cnt /= MS_PER_TICK; // 10 ms per tick;

    return delay_cnt;
}
