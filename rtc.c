#include "console.h"
#include "i386.h"
#include "irq.h"
#include "rtc.h"

#define CTC_RATE 0x0E // 4 bits, 32768 >> CTC_RATE
#define UTC_DIFF -2

static void set_system_time(uint16_t y, uint8_t m, uint8_t d, uint8_t h,
                            uint8_t i, uint8_t s)
{
    uint8_t mon_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t leaps, days, j;
    y -= 1970;
    leaps = (y + 2) / 4;
    m--;
    for (j = 0, days = 0; j < m; j++)
        days += mon_days[j];
    days += d - 1;
    days = days + (y * 365) + leaps;
    h = h + UTC_DIFF;
    system_time = (days * 86400) + (h * 3600) + (i * 60) + s;
}

/**
 * Read the CMOS register reg
 */
static uint8_t rtc_get_reg(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

/**
 * Real time clock interrupt handler. 
 * This will be called like four times a second.
 * Here we will read the current date time from CMOS,
 *  compute the current timestamp and save it to global var system_time
 */
void rtc_handler(struct iregs *r)
{
    uint8_t c, y, m, d, h, i, s, type, b;
    (void) type;
    (void) r;
    // 0x0C (type of interrupt) must be read, otherwise no int will be fired
    rtc_get_reg(0x0C);
    b = rtc_get_reg(0x0B);
    outb(0x70, 0x00);
    s = rtc_get_reg(0x00);
    i = rtc_get_reg(0x02);
    h = rtc_get_reg(0x04);
    d = rtc_get_reg(0x07);
    m = rtc_get_reg(0x08);
    y = rtc_get_reg(0x09);
    c = rtc_get_reg(0x32);
    // if cannot read century, assume 20
    if (c == 0)
        c = 20;
    if (!(b & 0x04)) {
        // BCD to binary conversion //
        s = (s & 0x0F) + ((s / 16) * 10);
        i = (m & 0x0F) + ((i / 16) * 10);
        h = ((h & 0x0F) + (((h & 0x70) / 16) * 10)) | (h & 0x80);
        d = (d & 0x0F) + ((d / 16) * 10);
        m = (m & 0x0F) + ((m / 16) * 10);
        y = (y & 0x0F) + ((y / 16) * 10);
        c = (c & 0x0F) + ((c / 16) * 10);
    }
    set_system_time(c*100+y, m, d, h, i, s);
}

/**
 * Installs the real time clock to fire int 8, handled by rtc_handler
 */
void rtc_init() 
{
    uint8_t prev_val;
    system_time = 0;

    cli();
    irq_install_handler(0x08, rtc_handler);
    // set frequency of interrupt //
    outb(0x70, 0x8A);
    prev_val = inb(0x71);
    outb(0x70, 0x8A);
    outb(0x71, (prev_val & 0xF0) | CTC_RATE); // set f
    // enable rtc interrupts //
    outb(0x70, 0x0B); // select register B
    prev_val = inb(0x71);
    outb(0x70, 0x0B);
    outb(0x71, prev_val | 0x40); // set bit 6, enable interrupt
    sti();
}
