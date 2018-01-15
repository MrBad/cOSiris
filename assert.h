#include "i386.h"
#include "console.h"

#define KASSERT(cond) do { \
    if (!(cond)) { \
        kprintf("Assertion: (%s) failed in %s(), at %s, line %d\n", \
        #cond, __func__,  __FILE__, __LINE__); \
        halt(); \
    } \
} while(0)
