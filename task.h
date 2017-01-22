#ifndef _TASK_H
#define _TASK_H


#include "mem.h"

typedef struct task {
	int pid;				// task + 0		process id
	int ppid;				// task + 4		parent pid
	unsigned int esp;		// task + 8		esp
	unsigned int ebp;		// task + 12	ebp
	unsigned int eip;		// task + 16	last eip / instruction pointer
	dir_t *page_directory;	// task + 20	task page directory
	struct task *next;		// task + 24	next task

	unsigned int *tss_kernel_stack; 	// f..k i386 tss thing

} task_t;

task_t *current_task;
task_t *task_queue;

int next_pid;


void task_init();
int getpid();
void task_switch();		// sched.asm
pid_t fork();	// sched.asm
void print_current_task();
void ps();
task_t *get_last_task();
task_t *get_next_task();
task_t *get_current_task();
void exit(int status);
#endif
