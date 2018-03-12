#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL ((void *)0)
#define offsetof(st, m) ((size_t)&(((st *)0)->m))

typedef int ptrdiff_t;

typedef unsigned int size_t;

//wchar_t

#endif
