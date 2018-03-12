#include <sys/types.h>

uint16_t _swap16(uint16_t num)
{
    return ((num & 0xFF) << 8) | ((num >> 8) & 0xFF);
}

