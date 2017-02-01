#ifndef _STDLIB_H
#define _STDLIB_H

#include "types.h"

void exit(int status);

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

#endif
