#ifndef _TASK_H
#define _TASK_H

#include "mem.h"
#include "kheap.h"
#include "list.h"
#include "x86.h"

typedef enum {
	TASK_CREATING,
	TASK_READY,
	TASK_SLEEPING,
	TASK_EXITING,
	TASK_EXITED,
} task_states_t;


typedef struct task {
	pid_t pid;				// task + 0		process id
	pid_t ppid;				// task + 4		parent pid
	unsigned int esp;		// task + 8		esp
	unsigned int ebp;		// task + 12	ebp
	unsigned int eip;		// task + 16	last eip / instruction pointer
	dir_t *page_directory;	// task + 20	task page directory
	struct task *next;		// task + 24	next task
	struct task *parent;	// parent task
	unsigned int *tss_kernel_stack;
	int ring;
	task_states_t state;
	int exit_status;
	list_t *wait_queue;
	heap_t * heap; 			// user heap
} task_t;


task_t *current_task;
task_t *task_queue;
spin_lock_t task_lock;

int next_pid;


extern void switch_to_user_mode_asm(unsigned int code_addr, unsigned int stack_hi_addr);

void task_init();
pid_t getpid();
task_t *get_task_by_pid(pid_t pid);
void task_switch();		// sched.asm
pid_t fork();			// sched.asm
void print_current_task();
task_t *get_last_task();
task_t *get_next_task();
task_t *get_current_task();
void task_exit(int status);
pid_t task_wait(int *status);
void ps();
// void exec_init();
void task_exec(char * path);

#endif
