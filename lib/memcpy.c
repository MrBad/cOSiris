#include <string.h>
void *memcpy(void *dest, void *src, size_t n) {
	size_t i;
	char *d = (char *) dest;
	const char *s = (const char *) src;

	for(i=0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}
