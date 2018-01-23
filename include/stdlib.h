#ifndef _STDLIB_H
#define _STDLIB_H

#include <sys/types.h>

void exit(int status);

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

int atoi(const char *str);

#endif
