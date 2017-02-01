#include <stdio.h>
#define assert(cond) do { \
	if (!(cond)) { \
		printf("Assertion: (%s) failed in %s(), at %s, line %d\n", \
		#cond, __func__,  __FILE__, __LINE__); \
		exit(1); \
	} \
} while(0)
