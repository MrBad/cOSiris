#include <string.h>

char * strncat(char *dest, char *src, unsigned int n)
{
	unsigned int dlen, i;
	dlen = strlen(dest);
	for(i = 0; i < n && src[i]; i++) {
		dest[dlen+i] = src[i];
	}
	dest[dlen+i] = 0;
	return dest;
}
