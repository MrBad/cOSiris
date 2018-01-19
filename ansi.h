#ifndef _ANSI_H
#define _ANSI_H

#include <sys/types.h>

// SeeAlso: https://en.wikipedia.org/wiki/ANSI_escape_code
// when not specified, n == 1
#define ANSI_CUU 'A'    // moves cursor n, (n), cells up
#define ANSI_CUD 'B'    // moves cursor n, (n), cells down
#define ANSI_CUF 'C'    // forward (n)
#define ANSI_CUB 'D'    // backward (n)
#define ANSI_CNL 'E'    // moves cursor to begining of next (n) line
#define ANSI_CPL 'F'    // moves cursor to begining of previous (n) line
#define ANSI_CHA 'G'    // moves cursor to horizontal (n)
#define ANSI_CUP 'H'

#define ANSI_ESC '\033'
#define ANSI_BRA '['
#define ANSI_MAX_CHARS 10

typedef enum {
    ANSI_NONE,      // machine is not in a escape sequence
    ANSI_IN_ESC,    // machine is in \033
    ANSI_IN_BRA,    // machine is in [
    ANSI_IN_NUM1,   // machine is processing first number
    ANSI_IN_NUM2,   // machine is processing second number after ;
    ANSI_IN_FIN,    // machine finished processing all
} ansi_stat_t;

/**
 * ANSI state machine
 */
struct ansi_stat {
    ansi_stat_t stat;   // current status
    uint32_t n1, n2;    // saved numbers after [
    char c1, c2;        // saved characters
    uint8_t cnt;        // how many loops since reset?
};

/**
 * An ANSI state machine switch.
 * Giving an existing status structure and a character c, 
 * moves the status in a new state.
 */
ansi_stat_t ansi_stat_switch(struct ansi_stat *stat, char c);

#endif
