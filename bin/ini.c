#include <stdio.h>
#include <stdlib.h>

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
    int i;
    // Free atexit stack
    for (i = 0; i < exit_stack_idx; i++)
        exit_stack[i] = 0;
    // Init buffered files IO structs
    _iob_init();
}
