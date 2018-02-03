#include <stdlib.h>

/**
 * Register a function to execute at program exit
 */
int atexit(void (*func)(void))
{
    if (exit_stack_idx > 0) {
        exit_stack[--exit_stack_idx] = func;
        return 0;
    }

    return -1;
}

