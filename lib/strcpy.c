#include <string.h>

char *strcpy(char *dest, const char *src) {
	unsigned int i;
	for(i=0; src[i] != 0; i++) {
		dest[i] = src[i];
	}
	dest[i] = 0;
	return dest;
}