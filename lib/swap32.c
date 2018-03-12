#include <string.h>
#include <sys/types.h>

uint32_t _swap32(uint32_t num)
{
    uint8_t data[4];
    memcpy(data, &num, sizeof(data));
    return ((uint32_t) data[3] << 0) 
         | ((uint32_t) data[2] << 8)
         | ((uint32_t) data[1] << 16)
         | ((uint32_t) data[0] << 24);
}

