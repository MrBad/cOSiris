#include <stdlib.h>

void _ini()
{
    int i;
    for (i = 0; i < exit_stack_idx; i++)
        exit_stack[i] = 0;
}
