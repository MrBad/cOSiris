#ifndef _TASK_H
#define _TASK_H

#include "mem.h"
#include "kheap.h"
#include "list.h"
#include "i386.h"
#include "isr.h"
#include "sys.h"
#include "sysfile.h"
#include "signal.h"

typedef enum {
    TASK_CREATING,
    TASK_RUNNING,
    TASK_AWAKEN,
    TASK_SLEEPING,
    TASK_EXITING,
} task_states_t;

#define TASK_INITIAL_NUM_FILES 6
#define MAX_OPEN_FILES 64
#define MAX_ARGUMENTS 16

typedef struct task task_t;

typedef struct task {
    pid_t pid;              // task + 0		process id
    pid_t ppid;             // task + 4		parent pid
    unsigned int esp;       // task + 8		esp
    unsigned int ebp;       // task + 12	ebp
    unsigned int eip;       // task + 16	last eip / instruction pointer
    dir_t *page_directory;  // task + 20	task page directory
    struct task *next;	    // task + 24	next task
    struct task *parent;    // parent task
    // uint32_t *tss_kernel_stack;
    unsigned int uid, gid;
    int ring;               // not making too much sense; all are in ring 3
    task_states_t state;
    void *sleep_addr;
    int exit_status;
    list_t *wait_queue;
    sighandler_t sig_handlers[NUM_SIGS]; // handlers for signals
    heap_t *heap;           // user heap
    char *name;             // yeah, let's name it!
    // file sistem //
    fs_node_t *root_dir;    // which is process root directory ? use in chroot
    char *cwd;              // current working directory
    struct file **files;    // pointer to num_files array
    int num_files;          // available number of slots < MAX_OPEN_FILES
    bool is_thread;
    task_t *s_task;
} task_t;

/**
 * Global vars
 */
task_t *current_task;       // current running task
task_t *task_queue;         // list of tasks
list_t *sigs_queue;         // list of signals
int next_pid;               // next available free pid number
bool switch_locked;

/**
 * Initializes the tasks
 */
void task_init();

/**
 * Sets kernel stack point
 */
void set_kernel_esp(uint32_t esp);

/**
 * Creates a new task
 */
task_t *task_new();

/**
 * Gets current running task pid
 */
pid_t getpid();

/**
 * Returns the task of pid
 */
task_t *get_task_by_pid(pid_t pid);

/**
 * Saves current task esp,ebp,eip, get next task, switch to it's page directory
 *  and jump to it's saved eip
 */
void task_switch();		// sched.asm

/**
 * Sets esp, ebp and jump to current task eip
 * Function is not returning!
 */
void jump_to_current_task(); // sched.asm

/**
 * Gets next task, switch to it's pd and jump to it's eip
 * Function is not returning!
 */
void task_switch_next();

/**
 * There is no spoon
 */
pid_t fork();			// sched.asm

/**
 * Dump current task
 */
void print_current_task();

/**
 * Gets the task in the tail
 */
task_t *get_last_task();

/**
 * Returns current task - TODO: we can wipe this, as current_task var is global
 */
task_t *get_current_task();

/**
 * Exit current task, with status
 */
void task_exit(int status);

/**
 * Suspends current process execution until
 * one of it's children terminates. 
 * Return the pid of child and it's exit status in status.
 */
pid_t task_wait(int *status);

/**
 * Debug - print task queue
 */
void ps();

/**
 * Jumps to ring 3, at code addres, having stack at stack_hi_addr
 */
void switch_to_user_mode(uint32_t code_addr, uint32_t stack_hi_addr);

/**
 * Put current task to sleep, on a particular address / event
 */
void sleep_on(void *addr);

/**
 * Wakes up all tasks that sleep on a particular address / event
 */
int wakeup(void *addr);

#endif
