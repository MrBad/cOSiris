#ifndef _TASK_H
#define _TASK_H


#include "mem.h"

typedef struct task {
	int pid;				// task + 0		process id
	unsigned int esp;		// task + 4		esp
	unsigned int ebp;		// task + 8		ebp
	unsigned int eip;		// task + 12	last eip
	dir_t *page_directory;	// task + 16	task page directory
	struct task *next;		// task + 20	next task
} task_t;

task_t *current_task;
task_t *task_queue;

int next_pid;


void task_init();
int getpid();
void task_switch();		// sched.asm
int fork();				// sched.asm


#endif
