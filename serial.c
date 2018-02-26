#include <stdio.h>
#include "i386.h"
#include "irq.h"
#include "console.h"
#include "serial.h"
#include "tty.h"
#include "kdbg.h"

// COM ports //
#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

/**
 * Transmit a character
 */
void serial_putc(char a, int tty_minor)
{
    int com;

    switch (tty_minor) {
        case TTYS0: com = COM1; break;
        case TTYS1: com = COM2; break;
        case TTYS2: com = COM3; break;
        case TTYS3: com = COM4; break;
    }

    while ((inb(com + 5) & 0x20) == 0)
        ;
    outb(com, a);
}

/**
 * Raw write the buffer, as it is, to serial line
 */
void serial_write(char *buf, int tty_minor)
{
    while(*buf) {
        serial_putc(*buf++, tty_minor);
    }
}

/**
 * Like kprintf, printf, but send the text to serial line
 */
void serial_debug(char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    serial_write(buf, TTYS0);
}

int serial_getc(int tty_minor)
{
    int com;
    switch (tty_minor) {
        case TTYS0: com = COM1; break;
        case TTYS1: com = COM2; break;
        case TTYS2: com = COM3; break;
        case TTYS3: com = COM4; break;
    }
    if(!(inb(com + 5) & 0x01))
        return -1;
    return inb(com);
}

/**
 * Serial interrupt handler (int 4, 3).
 * This is called when there is a characted in the serial buffer to be read.
 * Here will read the character, and push it to console handler.
 * The pass of the char is hacky right now - TODO - clean this.
 */
static void serial_handler(struct iregs *r)
{
    (void) r;
    char c, iir;
    int i;
    int coms[] = {COM1, COM2, COM3, COM4};
    for (i = 0; i < 4; i++) {
        iir = inb(coms[i] + 2);
        if ((iir & 0001) == 0) {
#ifdef KDEBG
            if (KDBG_TTY == TTYS1 + i)
                continue;
#endif
            c = inb(coms[i]);
            tty_in(tty_devs[TTYS0+i], c);
        }
    }
}

/**
 * Initialize the serial lines - COM1 right now.
 * COM1 is set at 38400 baud, 8 bits, one stop bit, no parity
 * I plan to use COM2 for general write, in the early stages, when 
 *   there is not irq installed, or to use it for gdb...
 */
void serial_enable(int port)
{
    cli();
    outb(port + 2, 0x02);         // Turn off FIFO.
    outb(port + 3, 0x80);         // Unlock divisor (DLAB = 1)
    outb(port + 0, 115200/38400); // Set 38400 baud, low byte
    outb(port + 1, 0);            //                 high byte
    outb(port + 3, 0x03);   // Lock divisor, 8 bits, no parity, one stop bit
    outb(port + 4, 0);
    outb(port + 1, 0x01);   // Enable interrupts

    if(inb(port + 5) == 0xFF)
        return;
    inb(port + 2);
    inb(port + 0);
    sti();
}

void serial_init()
{
    irq_install_handler(0x04, serial_handler);
    serial_enable(COM1);
    serial_enable(COM3);
    irq_install_handler(0x03, serial_handler);
    serial_enable(COM2);
    serial_enable(COM4);
}

