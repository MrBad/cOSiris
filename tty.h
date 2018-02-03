#ifndef _TTY_H
#define _TTY_H

#include <termios.h>
#include "ring_buf.h"

#define NTTY 12
#define NPTY 52
#define NTTYS 4
#define TTY_MAJOR 4
#define CTRL(key) (key - '@')

#define TTY_IN_BUF_SIZE 128
#define TTY_OUT_BUF_SIZE 128
#define TTY_LN_BUF_SIZE 512

typedef void (*putc_t)(char c);

typedef struct tty_line {
    char buf[TTY_LN_BUF_SIZE];
    int ed, len;
} tty_line_t;

typedef struct tty {
    struct winsize winsize;
    struct termios termios;
    ring_buf_t *cin;
    ring_buf_t *cout;
    tty_line_t line;
    putc_t putc;
    int minor;
    pid_t ctrl_pid; // pid of the controlling process
    pid_t fg_pid;   // pid of the foreground process
} tty_t;

tty_t *tty_devs[NTTY + NTTYS];
tty_t *curr_tty;

void tty_set_defaults(tty_t *tty);
void tty_in(tty_t *tty, char c);
void tty_open(fs_node_t *node, unsigned int flags);
void alt_tty_open(fs_node_t *node, unsigned int flags);

#endif

