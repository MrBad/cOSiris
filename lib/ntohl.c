#include <sys/types.h>

extern uint32_t _swap32(uint32_t);

uint32_t ntohl(uint32_t netlong)
{
    return _swap32(netlong);
}

