unsigned long timer_ticks;
#define HZ 100
#define MS_PER_TICK 1000/HZ

void timer_install(void);
void timer_wait(int ms);

