#include <sys/types.h>

extern uint32_t _swap32(uint32_t);

uint32_t htonl(uint32_t hostlong)
{
    return _swap32(hostlong);
}

