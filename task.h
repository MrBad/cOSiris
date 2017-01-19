#ifndef _TASK_H
#define _TASK_H

#include "mem.h"


typedef struct task {
	int pid;					// process id
	unsigned int esp, ebp;	// stak pointer, base pointer
	unsigned int eip;
	dir_t *page_directory;
	struct task *next;
} task_t;

task_t *current_task;
task_t *ready_queue;

int next_pid;


void task_init();
void task_switch();
int getpid();
int fork();

#endif
