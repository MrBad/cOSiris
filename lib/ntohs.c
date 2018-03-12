#include <sys/types.h>

extern uint16_t _swap16(uint16_t num);

uint16_t ntohs(uint16_t netlong)
{
    return _swap16(netlong);
}

