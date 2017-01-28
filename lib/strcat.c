#include <string.h>
char *strcat(char *dest, char *src)
{
	unsigned int dlen, i;
	dlen = strlen(dest);
	for(i = 0; src[i]; i++) {
		dest[dlen+i] = src[i];
	}
	dest[dlen+i] = 0;
	return dest;
}
