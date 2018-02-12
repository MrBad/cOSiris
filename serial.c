#include <stdio.h>
#include "i386.h"
#include "irq.h"
#include "console.h"
#include "serial.h"
#include "tty.h"

// COM ports //
#define COM1 0x3F8
#define COM2 0x2F8  

int serial_up;
/**
 * Transmit a character
 */
void serial_putc(char a)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
        ;
    outb(COM1, a);
}

/**
 * Raw write the buffer, as it is, to serial line
 */
void serial_write(char *buf)
{
    while(*buf) {
        serial_putc(*buf++);
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
    serial_write(buf);
}

int serial_getc()
{
    if(!(inb(COM1 + 5) & 0x01))
        return -1;
    return inb(COM1);
}

/**
 * Serial interrupt handler (int 4).
 * This is called when there is a characted in the serial buffer to be read.
 * Here will read the character, and push it to console handler.
 * The pass of the char is hacky right now - TODO - clean this.
 */
static void serial_handler(struct iregs *r)
{
    (void) r;
    char c = serial_getc();
    tty_in(tty_devs[12], c);
    //console_in(serial_getc);
}

/**
 * Initialize the serial lines - COM1 right now.
 * COM1 is set at 38400 baud, 8 bits, one stop bit, no parity
 * I plan to use COM2 for general write, in the early stages, when 
 *   there is not irq installed, or to use it for gdb...
 */
void serial_init() 
{
    cli();
    irq_install_handler(0x04, serial_handler);
    outb(COM1 + 2, 0x02);         // Turn off FIFO.
    outb(COM1 + 3, 0x80);         // Unlock divisor (DLAB = 1)
    outb(COM1 + 0, 115200/38400); // Set 38400 baud, low byte
    outb(COM1 + 1, 0);            //                 high byte
    outb(COM1 + 3, 0x03);   // Lock divisor, 8 bits, no parity, one stop bit
    outb(COM1 + 4, 0);
    outb(COM1 + 1, 0x01);   // Enable interrupts

    if(inb(COM1 + 5) == 0xFF)
        return;
    inb(COM1+2);
    inb(COM1+0);
    serial_write("Serial is up\n");
    sti();
}

