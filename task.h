#ifndef _TASK_H
#define _TASK_H

#include "mem.h"
#include "kheap.h"
#include "list.h"
#include "x86.h"
#include "sysfile.h"

typedef enum {
	TASK_CREATING,
	TASK_READY,
	TASK_SLEEPING,
	TASK_EXITING,
	TASK_EXITED,
} task_states_t;

#define TASK_INITIAL_NUM_FILES 3
#define MAX_OPEN_FILES 64
#define MAX_ARGUMENTS 16

typedef struct task {
	pid_t pid;				// task + 0		process id
	pid_t ppid;				// task + 4		parent pid
	unsigned int esp;		// task + 8		esp
	unsigned int ebp;		// task + 12	ebp
	unsigned int eip;		// task + 16	last eip / instruction pointer
	dir_t *page_directory;	// task + 20	task page directory
	struct task *next;		// task + 24	next task
	struct task *parent;	// parent task
	// uint32_t *tss_kernel_stack;
	unsigned int uid, gid;
	int ring;
	task_states_t state;
	void *sleep_addr;
	int exit_status;
	list_t *wait_queue;
	heap_t * heap; 			// user heap
	char *name;				// yeah, let's name it!
	// file sistem //
	fs_node_t *root_dir; 	// which is process root directory ? use in chroot
	char *cwd;		 		// current working directory
	struct file **files;	// pointer to num_files array
	int num_files;			// available number of slots < MAX_OPEN_FILES
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
void task_exec(char * path, char *argv[]);
void switch_to_user_mode(uint32_t code_addr, uint32_t stack_hi_addr);


void sleep_on(void *addr);
int wakeup(void *addr);
#endif
