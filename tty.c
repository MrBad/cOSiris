/**
 *  read ^ user program ^ write
 *       |              |
 *  +------------------------+
 *  |                        |
 *  | in_buf   TTY   out_buf | 
 *  |                        |
 *  +------------------------+
 *       ^              ^
 *       |  char device | 
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "console.h"
#include "serial.h"
#include "crt.h"
#include "assert.h"
#include "i386.h"
#include "vfs.h"
#include "syscall.h"
#include "task.h"
#include "signal.h"
#include "crt.h"
#include "tty.h"

static int tty_ioctl(fs_node_t *node, int request, void *argp)
{
    tty_t *tty = tty_devs[node->minor];
    if (!tty)
        return -1;
    KASSERT(node->major == TTY_MAJOR);

    switch (request) {

        // Get the current termios
        case TCGETS:
            if (!argp) return -1;
            validate_usr_ptr(argp);
            memcpy(argp, &tty->termios, sizeof(struct termios));
            return 0;

        // Set the current termios
        case TCSETS:
            if (!argp) return -1;
            validate_usr_ptr(argp);
            memcpy(&tty->termios, argp, sizeof(struct termios));
            return 0;

        // Get window size
        case TIOCGWINSZ:
            if (!argp) return -1;
            validate_usr_ptr(argp);
            memcpy(argp, &tty->winsize, sizeof(struct winsize));
            return 0;

        // Set window size
        case TIOCSWINSZ:
            if (!argp) return -1;
            validate_usr_ptr(argp);
            memcpy(&tty->winsize, argp, sizeof(struct winsize));
            return 0;

        // Get the process group ID of the foreground process group ID
        case TIOCGPGRP:
            if (!argp) return -1;
            validate_usr_ptr(argp);
            *(pid_t *)argp = tty->fg_pid;
            return 0;

        // Set the foreground process group ID of this terminal.
        case TIOCSPGRP:
            if (!argp) return -1;
            validate_usr_ptr(argp);
            tty->fg_pid = *(pid_t *)argp;
            return 0;

        default:
            return -1;
    }
}

void tty_oput(tty_t *tty, char c)
{
    // TODO: Put to cout buffer; right now, output is unbuffered
    if (tty->termios.c_oflag & OPOST) {
        if (c == '\n') {
            if (tty->termios.c_oflag & ONLCR)
                tty->putc('\r', tty->minor);
        }
    }
    tty->putc(c, tty->minor);
}

void tty_oputs(tty_t *tty, char *s)
{
    while (*s)
        tty_oput(tty, *s++);
}

void tty_iput(tty_t *tty, char c)
{
    rb_putc(tty->cin, c);
    if (rb_is_full(tty->cin) || c == '\n' 
        || (!(tty->termios.c_lflag & ICANON))) {
        wakeup(&tty->cin);
        task_switch();
    }
}

void tty_echo(tty_t *tty, char c)
{
    if (c >= ' ' || c == '\n') {
        tty_oput(tty, c);
    } else { 
        tty_oput(tty, '^');
        tty_oput(tty, c + '@');
    }
}

void tty_editor(tty_t *tty, char c)
{
    KASSERT(tty);
    tcflag_t lf = tty->termios.c_lflag;
    cc_t *cc = &(tty->termios.c_cc[0]);
    tty_line_t *ln = &(tty->line);
    int i;

    if (c == cc[VEOL] || c == cc[VEOL2] || c == '\n' 
            || (uint32_t)ln->len >= sizeof(ln->buf) - 1) {
        if (lf & ECHO)
            tty_oput(tty, c);
        for (i = 0; i < ln->len; i++)
            tty_iput(tty, ln->buf[i]);
        tty_iput(tty, c);
        ln->ed = ln->len = ln->buf[0] = 0;
        return;
    } else if (c == cc[VERASE]) {
        if (ln->ed == 0)
            return;
        memmove(ln->buf + ln->ed - 1, ln->buf + ln->ed, ln->len - ln->ed + 1);
        ln->ed--;
        ln->len--;
        if (lf & ECHO) {
            if (ln->ed == ln->len) {
                tty_oputs(tty, "\b \b");
            } else {
                tty_oputs(tty, "\033[1D");
                tty_oputs(tty, "\033[s");
                for (i = ln->ed; i < ln->len; i++)
                    tty_echo(tty, ln->buf[i]);
                tty_oput(tty, ' ');
                tty_oputs(tty, "\033[u");
            }
        }
        return;
    } else if (c == cc[VKILL]) {
        ln->ed = ln->len = ln->buf[0] = 0;
        if (lf & ECHO)
            tty_oputs(tty, "\033[G\033[2K");
        return;
    } else if (c == cc[VINTR]) {
        ln->ed = ln->len = ln->buf[0] = 0;
        if (tty->fg_pid) 
            kill(tty->fg_pid, SIGINT);
    } else if (c == cc[VQUIT]) {
        ln->ed = ln->len = ln->buf[0] = 0;
        if (tty->fg_pid)
            kill(tty->fg_pid, SIGQUIT);
    } else if (c == cc[VEOF]) {
        if (ln->len) {
            for (i = 0; i< ln->len; i++)
                tty_iput(tty, ln->buf[i]);
            ln->ed = ln->len = ln->buf[0] = 0;
        }
        wakeup(&tty->cin);
    } else if (c == cc[VSUSP]) {
        if (tty->fg_pid)
            kill(tty->fg_pid, SIGTSTP);
    } else if (c == cc[VSTART]) {
        if (tty->fg_pid)
            kill(tty->fg_pid, SIGCONT);
    } else if (c == CTRL('L')) {
        tty_set_defaults(tty);
        tty_oputs(tty, "\033c");
    } else if (c == CTRL('P')) {
        ps();
        return;
    } else if (c == CTRL('F')) {
        kprintf("fg_pid: %d\n", tty->fg_pid);
        return;
    } else if (c == CTRL('O')) {
        cofs_dump_cache();
        return;
    } else {
        if (ln->ed == ln->len) {
            ln->buf[ln->ed++] = c;
            ln->len++;
        } else {
            memmove(ln->buf + ln->ed + 1, ln->buf + ln->ed, ln->len - ln->ed);
            ln->buf[ln->ed++] = c;
            ln->len++;
        }
    }
    if (lf & ECHO)
        tty_echo(tty, c);
}

/**
 * Called from interrupt
 */
void tty_in(tty_t *tty, char c)
{
    tcflag_t ifl, loc;
    if (!tty)
        return;
    ifl = tty->termios.c_iflag;
    loc = tty->termios.c_lflag;

    if (c == '\n') {
        if (ifl & INLCR)
            c = '\r';
    } else if (c == '\r') {
        if (ifl & IGNCR)
            return;
        else if (ifl & ICRNL)
            c = '\n';
    }
    if (ifl & ISTRIP)
        c &= 0x7F;
    if (loc & ICANON) {
        tty_editor(tty, c);
    } else  {
        if (loc & ECHO)
            tty_echo(tty, c);
        tty_iput(tty, c);
    }
}

uint32_t tty_write(fs_node_t *node, uint32_t offs, uint32_t size, char *buf)
{
    (void) offs;
    uint32_t i;
    tty_t *tty = tty_devs[node->minor];
    if (!tty)
        return -1;
    KASSERT(node->major == TTY_MAJOR);

    for (i = 0; i < size; i++)
        tty_oput(tty, buf[i]);

    return size;
}

uint32_t tty_read(fs_node_t *node, uint32_t offs, uint32_t size, char *buf)
{
    (void) offs;
    char c;
    uint32_t num_bytes = 0;
    tty_t *tty = tty_devs[node->minor];
    if (!tty)
        return -1;
    KASSERT(node->major == TTY_MAJOR);
    while (num_bytes < size) {
        if (rb_is_empty(tty->cin)) {
            if (current_task->state == TASK_EXITING)
                return -1;
            if (current_task->state == TASK_RUNNING)
                sleep_on(&tty->cin);
        }
        if (rb_is_empty(tty->cin))
            break;
        c = rb_getc(tty->cin);
        buf[num_bytes++] = c;
        if (c == '\n' || ((tty->termios.c_lflag & ICANON) == 0))
            break;
    }
    return num_bytes;
}

/**
 * Set tty default parameters
 */
void tty_set_defaults(tty_t *tty)
{
    tty->termios.c_iflag = ICRNL | BRKINT;
    tty->termios.c_oflag = ONLCR | OPOST;
    tty->termios.c_lflag = ECHO | ECHOE | ECHOK | ISIG | IEXTEN | ICANON;
    tty->termios.c_cflag = CREAD;
    tty->termios.c_cc[VINTR] = CTRL('X');
    tty->termios.c_cc[VQUIT] = CTRL('\\');
    tty->termios.c_cc[VERASE] = '\b';
    tty->termios.c_cc[VKILL] = CTRL('U');
    tty->termios.c_cc[VEOF] = CTRL('D');
    tty->termios.c_cc[VTIME] = 0;
    tty->termios.c_cc[VMIN] = 1;
    tty->termios.c_cc[VSTART] = CTRL('Q');
    tty->termios.c_cc[VSTOP] = CTRL('S');
    tty->termios.c_cc[VSUSP] = CTRL('Z');
    tty->termios.c_cc[VEOL] = '\n';
    tty->termios.c_cc[VREPRINT] = CTRL('R');
    tty->termios.c_cc[VWERASE] = CTRL('W');
    tty->termios.c_cc[VLNEXT] = CTRL('V');

    rb_reset(tty->cin);
    rb_reset(tty->cout);
}

/**
 * Close the tty and if there is no reference to it, free tty structure.
 */
void tty_close(fs_node_t *node)
{
    tty_t *tty = tty_devs[node->minor];
    KASSERT(node->major == TTY_MAJOR);
    if (node->lock)
        spin_unlock(&node->lock);
    node->ref_count--;
    if (tty && node->ref_count == 0) {
        // No process use this tty anymore.
        rb_delete(tty->cin);
        rb_delete(tty->cout);
        free(tty);
        tty_devs[node->minor] = tty = NULL;
    }
}

/**
 * Alocate the tty structure into tty_devs if does not exists
 *  and attach the io_ops to the node.
 */
void tty_open(fs_node_t *node, uint32_t flags)
{
    (void) flags;
    tty_t *tty;

    KASSERT(node->major == TTY_MAJOR);

    if (!(tty = tty_devs[node->minor])) {
        if (!(tty = calloc(1, sizeof(*tty)))) {
            kprintf("tty_open: out of memory\n");
            return;
        }
        if (!(tty->cin = rb_new(TTY_IN_BUF_SIZE))) {
            free(tty);
            return;
        }
        if (!(tty->cout = rb_new(TTY_OUT_BUF_SIZE))) {
            rb_delete(tty->cin);
            free(tty);
            return;
        }
        tty->ctrl_pid = current_task->sid;
        tty->fg_pid = current_task->pid;
        tty->minor = node->minor;
        if (tty->minor < NTTY) {
            tty->putc = crt_putc;
            tty->winsize.ws_col = CRT_COLS;
            tty->winsize.ws_row = CRT_ROWS;
        } else {
            tty->putc = serial_putc;
        }
        tty_devs[node->minor] = tty;
        tty_set_defaults(tty);
    }
    // Attach the ops to the original fs_node.
    node->read = tty_read;
    node->write = tty_write;
    node->close = tty_close;
    node->ioctl = tty_ioctl;
}

void alt_tty_open(fs_node_t *node, uint32_t flags)
{
    if (node->minor == 1) {
        console_open(node, flags);
    }
}

