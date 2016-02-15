#include <string.h>

void *memset(void *s, int c, size_t n) {
	unsigned char *p = s;
	while(n > 0){
		*p++ = c;
		n--;
	}
	return s;
}