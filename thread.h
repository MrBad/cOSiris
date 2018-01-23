#ifndef _THREAD_H
#define _THREAD_H

#include "task.h"

typedef int (*thread_fn)(void *);

task_t *thread_new(thread_fn fn, void *arg);

void thread_free(task_t *t);

#endif
