#include <sys/types.h>

extern uint16_t _swap16(uint16_t num);

uint16_t htons(uint16_t hostshort)
{
    return _swap16(hostshort);
}

