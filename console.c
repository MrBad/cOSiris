#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "vfs.h"
#include "i386.h"
#include "task.h"
#include "pipe.h"
#include "kbd.h"
#include "crt.h"
#include "serial.h"
#include "console.h"
#include "ansi.h"
#include "signal.h"

spin_lock_t console_lock;
struct cin cin = {
    .cb.sz = CIN_BUF_SIZE,
};

/**
 * Puts a character at the current cursor position
 */
void console_putc(char c)
{
    serial_putc(c, TTYS0);
    crt_putc(c, TTY0);
}

/**
 * Write a string to console
 */
static void console_puts(char *str)
{
    while (*str) {
        if (*str == '\n') {
            console_putc('\r');
        }
        console_putc(*str++);
    }
}

/**
 * Write the string to all consoles and halt kernel
 */
void panic(char *str, ...)
{
    char buf[512];
    va_list args;

    va_start(args, str);
    vsnprintf(buf, sizeof(buf), str, args);
    va_end(args);
    // spin_lock(&console_lock);
    console_puts(buf);
    cli();
    halt();
}

/**
 * Kernel low level, unbuffered printf
 *  to be used before any tty is up
 *  or better, search for first tty somehow
 */
void kprintf(char *fmt, ...)
{
    char buf[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    spin_lock(&console_lock);
    console_puts(buf);
    spin_unlock(&console_lock);
}

/**
 * Send backspace
 */
void console_bs() 
{
    char seq[4] = "\b \b";
    console_puts(seq);
}

/**
 * Send ANSI cursor movement to out
 */
void console_cur_up(int n)
{
    char buf[32] = "\033[A";
    if (n)
        snprintf(buf, sizeof(buf), "\033[%dA", n);
    console_puts(buf);
}

void console_cur_down(int n)
{
    char buf[32] = "\033[B";
    if (n)
        snprintf(buf, sizeof(buf), "\033[%dB", n);
    console_puts(buf);
}

void console_cur_left(int n)
{
    char buf[32] = "\033[D";
    if (n)
        snprintf(buf, sizeof(buf), "\033[%dD", n);
    console_puts(buf);
}

void console_cur_right(int n)
{
    char buf[32] = "\033[C";
    if (n)
        snprintf(buf, sizeof(buf), "\033[%dC", n);
    console_puts(buf);
}

void console_clrscr()
{
    char buf[32] = "\033c";
    console_puts(buf);
}

/**
 * Actions to take when we receive a control char
 */
static void console_control(char c)
{
    int i;
   
    // CTRL+L pressed
    if (c == 'L') {
        console_clrscr();
    } else if (c == 'A') {
        // move edit cursor back to start of the line
        if (cin.cb.e > cin.cb.w) {
            console_cur_left(cin.cb.e - cin.cb.w);
            cin.cb.e = cin.cb.w;
        }
    } else if (c == 'U') {
        // kill line; erase from cursor to start of the line
        // can we simplify this?
        if (cin.cb.e > cin.cb.w) {
            memcpy(cin.cb.buf + cin.cb.w, cin.cb.buf + cin.cb.e, 
                   cin.cb.t - cin.cb.e);
            console_cur_left(cin.cb.e - cin.cb.w);
            for (i = 0; i < cin.cb.t - cin.cb.e; i++)
                console_putc(cin.cb.buf[cin.cb.w + i]);
            for (i = 0; i < cin.cb.e - cin.cb.w; i++)
                console_putc(' ');
            console_cur_left(cin.cb.t - cin.cb.w);
            cin.cb.t -= cin.cb.e - cin.cb.w;
            cin.cb.e = cin.cb.w;
        }
    } else if (c == 'P') {
        ps();
    } else if (c == 'X') {
        kprintf("^X");
        kill(current_task->pid, SIGINT);
    } else if (c == 'D') {
        wakeup(&console_read);
    } else if (c == 'Z') {
        kill(current_task->pid, SIGTSTP);
    } else if (c == 'Q') {
        kill(current_task->pid, SIGCONT);
    } else {
        kprintf("[Unavailable ^0x%X]\n", c);
    }
}

void console_scroll_up()
{
    crt_scroll_up();
    //serial_write("\033[10U");
}

void console_scroll_down()
{
    crt_scroll_down();
    //serial_write("\033[10C");
}

/**
 * If we parsed correctly the ansi escape sequence, execute it 
 *  if it is handled
 *  https://www.gnu.org/software/screen/manual/html_node/Input-Translation.html
 */
static void cin_handle_ansi()
{
    if (cin.astat.c1 == 'D' && cin.cb.e > cin.cb.w) {
        // move cursor back one position, until start of line(buf;w)
        cin.cb.e -= 1;
        console_cur_left(1);
    } else if (cin.astat.c1 == 'C' && cin.cb.e < cin.cb.t) {
        // forward cursor position, until end  of line (buf;t, top)
        cin.cb.e += 1;
        console_cur_right(1);
    } else if (cin.astat.c1 == '~') {
        // doing a stub for now, get what we use
        switch (cin.astat.n1) {
            //case 1: // home
            //case 4: // end
            //case 2: // insert
            //case 3: // delete
            case 5: // page up
                console_scroll_up();
                break;
            case 6: // page down
                console_scroll_down();
                break;
            case 15: // F5->
            case 18: // F7 ...
            case 24: // F12
            default:
                serial_debug("^%d~\n", cin.astat.n1);
                break;
        }
    } else {
        serial_debug("---^[%d;%d%c%c\n", 
                cin.astat.n1, cin.astat.n2, cin.astat.c1, cin.astat.c2);
    }
}

/**
 * Where the characters come in, from keyboard or serial line
 * TODO: console_in should not parse ansi escape sequences?!?!?!?!
 * We will move the readline part in the shell, and console will only forward 
 * escape sequences.
 * This way, ansi escapes like cursor up will be forwarded to shell (cosh) and
 * gives it the chance to implement history for example.
 * That's it, we will speak raw ANSI and leve the user programs interpret 
 * them.
 */
void console_in(getc_t getc)
{
    int c, i, ipos;
    //spin_lock(&console_lock);
    while((c = getc()) > 0) {
        //serial_debug("0x%X, %c\n", c, c);
        // ANSI esquape sequence?
        if (ansi_stat_switch(&cin.astat, c)) {
            if (cin.astat.stat == ANSI_IN_FIN) {
                cin_handle_ansi();
            }
            continue;
        }
        c = (c == '\r') ? '\n' : c;
        if (c < ' ' && c != '\n' && c != '\b' && c != '\t') {
            console_control(UC(c));
        }
        // Backspace? (can be in the middle of the edit line)
        else if (c == '\b') { 
            if (cin.cb.e - cin.cb.w > 0) {
                cin.cb.e--; cin.cb.t--;
                console_cur_left(1);
                for (i = cin.cb.e; i < cin.cb.t; i++) {
                    cin.cb.buf[i % cin.cb.sz] = cin.cb.buf[(i+1) % cin.cb.sz];
                    console_putc(cin.cb.buf[i%cin.cb.sz]);
                }
                console_putc(' ');
                console_cur_left(cin.cb.t-cin.cb.e + 1);
            }
        }
        // Treat it like a normal character
        else {
            if (cin.cb.t - cin.cb.r < cin.cb.sz) {
                if (cin.cb.t > cin.cb.e && c != '\n') {
                    for (i = cin.cb.t; i > cin.cb.e; i--)
                        cin.cb.buf[i%cin.cb.sz] = cin.cb.buf[(i - 1)%cin.cb.sz];
                }
                // Force \n to be added last in line always
                ipos = (c == '\n') ? cin.cb.t : cin.cb.e;
                cin.cb.buf[ipos % cin.cb.sz] = c;
                cin.cb.t++; cin.cb.e++;
                // Echo character(s) / repaint.
                for (i = cin.cb.e - 1; i < cin.cb.t; i++)
                    console_putc(cin.cb.buf[i % cin.cb.sz]);
                if (cin.cb.t > cin.cb.e)
                    console_cur_left(cin.cb.t - cin.cb.e);
                // If buffer is full or is a new line, wakeup tasks waiting.
                if (c == '\n' || cin.cb.t == cin.cb.r + cin.cb.sz) {
                    cin.cb.w = cin.cb.e = cin.cb.t;
                    wakeup(&console_read);
                }
            }

        }
    }
}

/**
 * Read from the console cin buffer, stdin
 */
unsigned int console_read(fs_node_t *node, unsigned int offset,
        unsigned int size, char *buffer)
{
    (void) node;
    (void) offset;
    unsigned char c;
    unsigned initial_size = size;
    // spin_lock(&console_lock);
    while (size > 0) {
        if (cin.cb.r == cin.cb.w) {
            if (current_task->state == TASK_EXITING) {
                //spin_unlock(&console_lock);
                return -1;
            }
            if (current_task->state == TASK_RUNNING) {
                sleep_on(&console_read);
            }
        }
        // Did we waked up by CTRL+D
        if (cin.cb.r == cin.cb.w)
            break;
        c = cin.cb.buf[cin.cb.r++ % cin.cb.sz];
        *buffer++ = c;
        size--;
        if (c == '\n') 
            break;
    }
    // spin_unlock(&console_lock);
    return initial_size - size;
}

/**
 * Unbuffered write to cout, stdout.
 * TODO: maybe we should have a buffer that we flush on the screen.
 */
unsigned int console_write(fs_node_t *node, unsigned int offset,
        unsigned int size, char *buffer)
{
    (void) node;
    (void) offset;
    unsigned int i;
    // spin_lock(&console_lock);
    for (i = 0; i < size; i++) {
        console_putc(buffer[i]);
    }
    // spin_unlock(&console_lock);
    return i;
}

void console_close(fs_node_t *node)
{
    node->ref_count--;
}

void console_open(fs_node_t *cons, uint32_t flags)
{
    (void) flags;
    cons->read = console_read;
    cons->write = console_write;
    cons->close = console_close;
}

