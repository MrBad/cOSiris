#include <string.h>

size_t strlen(const char *s) {
	size_t i;
	i = 0;
	while (*s++ != '\0') {
		i++;
	}
	return i;
}