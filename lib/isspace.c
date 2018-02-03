#include <string.h>

int isspace(int c)
{
    const char space[] = " \f\n\r\t\v";
    return strchr(space, c) ? 1 : 0;
}
