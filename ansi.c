#include <string.h>
#include "ansi.h"

/**
 * Try to switch the ANSI state machine, if possible
 *  by giving current state and a new character,
 *  return the new state, or 0 (ANSI_NONE) if is not in an ANSI transition
 */
ansi_stat_t ansi_stat_switch(struct ansi_stat *st, char c)
{
    st->cnt++;
    if (st->stat == ANSI_IN_FIN || st->cnt > ANSI_MAX_CHARS) {
        memset(st, 0, sizeof(*st)); // reset the machine
    }
    if (st->stat == ANSI_NONE && c == ANSI_ESC)
        return st->stat = ANSI_IN_ESC;
    
    if ((st->stat == ANSI_IN_ESC) && (c == ANSI_BRA || c == 'O' || c == 'c')) {
        if (c == 'O')
            st->c2 = 'O';
        else if (c == 'c') {
            st->c1 = c;
            return st->stat = ANSI_IN_FIN;
        }
        return st->stat = ANSI_IN_BRA;
    }
    if (st->stat == ANSI_IN_BRA && c >= '0' && c <= '9') {
        st->n1 = c - '0'; 
        return st->stat = ANSI_IN_NUM1;
    }
    if (st->stat == ANSI_IN_NUM1 && c >= '0' && c <= '9') {
        st->n1 = st->n1 * 10 + c - '0';
        return st->stat;
    }
    if (st->stat == ANSI_IN_NUM1 && c == ';') {
        st->n2 = 0;
        return st->stat = ANSI_IN_NUM2;
    }
    if (st->stat == ANSI_IN_NUM2 && c >= '0' && c <= '9') {
        st->n2 = st->n2 * 10 + c - '0'; 
        return st->stat;
    }
    if (st->stat > ANSI_IN_ESC && st->stat < ANSI_IN_FIN
        && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c=='~')) 
    {
        st->c1 = c;
        return st->stat = ANSI_IN_FIN;
    }
    st->cnt = 0;
    return st->stat = ANSI_NONE;
}

