#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** 
 * Per process, global vars
 *  libc implementation expects thease
 */
int exit_stack_idx = ATEXIT_MAX;
void(*exit_stack[ATEXIT_MAX])(void);

struct _iob_head *_iob = NULL;
extern void _iob_init();

void _ini()
{
    // Free atexit stack
    memset(exit_stack, 0, sizeof(exit_stack));
    _iob_init();
}
