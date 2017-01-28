#include <string.h>
extern void * malloc(unsigned int len);

char * strdup(char * src)
{
    char *ret;
    int len = strlen(src);

    ret = (char *) malloc(len + 1);
	strcpy(ret, src);
	ret[len] = 0;
    return ret;
}
