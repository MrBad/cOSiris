#include <sys/types.h>
// #include "assert.h"

char *strcpy(char *dest, char *src);
char * strncpy(char *dest, const char *src, unsigned int n);
int *strcmp(char *s1, char *s2);
void *memcpy(void *dest, void *src, size_t n);
// void* memcpy(void* dstptr, const void* srcptr, size_t size);
size_t strlen(const char *s);
void * memset(void *s, int c, size_t n);
void * memmove(void *mdest, void *msrc, int n);
char * strdup(char * src);
char * strcat(char *dest, char *src);
char * strncat(char *dest, char *src, unsigned int n);
