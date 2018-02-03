#include <sys/types.h>

unsigned int major(dev_t dev)
{
    return (dev & 0xFF00) >> 8;
}

unsigned int minor(dev_t dev)
{
    return dev & 0xFF;
}

dev_t makedev(int maj, int min)
{
    return ((maj & 0xFF) << 8) | (min & 0xFF);
}

