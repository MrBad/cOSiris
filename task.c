#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "assert.h"
#include "i386.h"
#include "mem.h"
#include "isr.h"
#include "console.h"
#include "gdt.h"
#include "assert.h"
#include "vfs.h"
#include "pipe.h"
#include "cofs.h"
#include "task.h"

bool switch_locked = false;
char *task_states[] = {
    "TASK_CREATING",
    "TASK_READY",
    "TASK_SLEEPING",
    "TASK_EXITING",
    "TASK_EXITED",
    0
};

/**
 * Creates a new task structure
 */
task_t *task_new()
{
    task_t *t = (task_t *) calloc(1, sizeof(task_t));
    t->pid = next_pid++;
    // t->tss_kernel_stack = (unsigned int *)KERNEL_STACK_HI;
    t->wait_queue = list_open(NULL);
    // heap //
    heap_t *h = calloc(1, sizeof(heap_t));
    h->start_addr = h->end_addr = UHEAP_START;
    h->max_addr = UHEAP_END;
    h->readonly = h->supervisor = false;
    t->heap = h;
    t->name = NULL;

    return t;
}

/**
 * Initialize multitasking
 */
void task_init()
{
    int i;
    struct file *f;
    cli();
    next_pid = 1;
    write_tss(5, 0x10, KERNEL_STACK_HI);
    gdt_flush();
    tss_flush();

    current_task = task_new();
    current_task->uid = current_task->gid = 0;
    current_task->page_directory = (dir_t *) virt_to_phys(PDIR_ADDR);
    current_task->root_dir = fs_root;
    current_task->cwd = strdup("/");
    current_task->name = strdup("init");
    current_task->state = TASK_READY;
    task_queue = current_task;

    // alloc files for first process //
    current_task->files = (struct file **) calloc(TASK_INITIAL_NUM_FILES, 
            sizeof(struct file *));
    current_task->num_files = TASK_INITIAL_NUM_FILES;
    for (i = 0; i < 3; i++) {
        f = calloc(1, sizeof(*f));
        f->fs_node = fs_namei("/dev/console");
        f->mode = (i == 0 ? O_RDONLY : O_WRONLY);
        current_task->files[i] = f;
    }
    sti();
}

void print_current_task()
{
    kprintf("current_task pid: %d, eip: %08x, esp:%08x, ebp: %08x, pd: %08x\n",
            current_task->pid, current_task->eip, current_task->esp, 
            current_task->ebp, current_task->page_directory);
}

void ps()
{
    task_t *t = task_queue;
    while (t) {
        // no eip, esp - assumes we are already in kernel mode //
        kprintf("%10s, pid: %2d, ppid: %2d, state: %2s, ring: %2d\n", 
                t->name ? t->name : "[unnamed]", t->pid, t->ppid, 
                task_states[t->state], t->ring);
        t = t->next;
    }
}

/**
 * Gets the last task (tail) in task_queue
 */
task_t *get_last_task()
{
    switch_locked = true;
    task_t *t = task_queue;
    while (t->next) {
        t = t->next;
    }
    switch_locked = false;
    return t;
}

task_t *task_switch_inner()
{
    task_t *n;

    if (switch_locked) {
        return current_task;
    }
    for (n = current_task->next; n; n = n->next) {
        if (n->state == TASK_READY)
            break;
    }
    if (!n) {
        for (n = task_queue; n; n = n->next)
            if (n->state == TASK_READY)
                break;
    }
    // giveup and return init
    current_task = n ? n : get_task_by_pid(1);
    current_task->state = TASK_READY;
    return current_task;
}

/**
 * Called from sched.asm
 */
task_t *fork_inner()
{
    int fd;
    switch_locked = true;
    task_t *t = task_new();
    t->ppid = current_task->pid;
    t->parent = current_task;
    t->ring = current_task->ring;
    t->uid = current_task->uid;
    t->gid = current_task->gid;
    t->page_directory = clone_directory();

    // child has end addr as parent, clonned by clone directory //
    t->heap->end_addr = current_task->heap->end_addr;

    // files //
    t->root_dir = current_task->root_dir;
    t->cwd = strdup(current_task->cwd);
    // clone files //
    t->files = (struct file **) calloc(current_task->num_files, sizeof(struct file *));
    for (fd = 0; fd < current_task->num_files; fd++) {
        if (!current_task->files[fd]) {
            continue;
        }
        t->files[fd] = calloc(1, sizeof(struct file));
        if (!current_task->files[fd]->fs_node) {
            panic("task fork: pid: %d, fd: %d fs_node does not exists\n", 
                    current_task->pid, fd);
        }
        t->files[fd]->fs_node = fs_dup(current_task->files[fd]->fs_node);

        if (t->files[fd]->fs_node->type & FS_PIPE) {
            vfs_pipe_t *pipe = (vfs_pipe_t*) t->files[fd]->fs_node->ptr;
            if (t->files[fd]->fs_node->flags == O_RDONLY) {
                pipe->readers++;
            } else {
                pipe->writers++;
            }
        }
        t->files[fd]->offs = current_task->files[fd]->offs;
    }
    t->num_files = current_task->num_files;

    t->state = TASK_READY;
    switch_locked = false;
    return t;
}

pid_t getpid()
{
    return current_task ? current_task->pid : -1;
}

int getring()
{
    return current_task ? current_task->ring : -1;
}

task_t *get_task_by_pid(pid_t pid)
{
    task_t *t = NULL;
 
    for (t = task_queue; t; t = t->next)
        if (t->pid == pid)
            break;

    return t;
}

/**
 * Free task alocated memory
 */
void task_free(task_t *task)
{
    KASSERT(task);
    task_t *p; int fd;
    for (p = task_queue; p; p = p->next) {
        if (p->next == task) {
            p->next = task->next;
        }
    }
    KASSERT(task->wait_queue->num_items == 0);
    free_directory(task->page_directory);
    // closing wait queue //
    list_close(task->wait_queue);

    // closing it's files //
    for (fd = 0; fd < task->num_files; fd++) {
        if (task->files[fd]) {
            KASSERT(task->files[fd]->fs_node);
            fs_close(task->files[fd]->fs_node);
            //	task->files[fd]->fs_node->ref_count--;
            if (task->files[fd]->dup_cnt == 0)
                free(task->files[fd]);
            else
                task->files[fd]->dup_cnt--;
        }
    }
    free(task->files);
    free(task->heap);
    if (task->name)
        free(task->name);
    free(task);
}

void sleep_on(void *addr)
{
    switch_locked = true;
    current_task->state = TASK_SLEEPING;
    current_task->sleep_addr = addr;
    switch_locked = false;

    task_switch();
}

/**
 * Waking up tasks that sleeps on a address
 */
int wakeup(void *addr)
{
    int cnt = 0;
    task_t * t;
    switch_locked = true;
    for (t = task_queue; t; t = t->next) {
        if (t->state == TASK_SLEEPING && t->sleep_addr == addr) {
            cnt++; t->state = TASK_READY;
            t->sleep_addr = 0;
        }
    }
    switch_locked = false;
    return cnt;
}

pid_t task_wait(int *status)
{
    task_t *t;
    node_t *n;
    pid_t pid;

    if (current_task->wait_queue->num_items == 0) {
        switch_locked = true;
        // check if this task has children to wait for //
        for (t = task_queue; t; t = t->next)
            if (t->ppid == current_task->pid)
                break;

        if (!t) {
            switch_locked = false;
            return -1;
        } 
        sleep_on(&current_task->wait_queue);
    }
    switch_locked = true;
    // This is init, because we forced it to wake up
    if (current_task->wait_queue->num_items == 0 && current_task->pid == 1) {
        switch_locked = false;
        return -1;
    }
    n = current_task->wait_queue->head;
    t = (task_t *) n->data;
    pid = t->pid;
    if (status) 
        *status = t->exit_status;
    task_free(t);
    list_del(current_task->wait_queue, n);
    switch_locked = false;
 
    return pid;
}

// When a task exits, it's resources are not freed until it's parent exits.
void task_exit(int status)
{
    task_t *t, *parent;
    node_t *n;

    if(!(parent = get_task_by_pid(current_task->ppid))) {
        // Probably we should reparent myself to init? But what if i am init?
        panic("pid: %d - i have no parent\n", current_task->pid);
        return;
    }
    // Reparent my children to init, it will clean them.
    for (t = task_queue; t; t = t->next)
        if (t->ppid == current_task->pid)
            t->ppid = 1;
    
    // Clean my waiting queue. 
    // All childs in waiting queue are in TASK_EXIT, so we free them
    switch_locked = true;
    for (n = current_task->wait_queue->head; n; n = n->next) {
        task_free((task_t *) n->data);
        list_del(current_task->wait_queue, n);
    }
    // Add me to the parent waiting queue //
    list_add(parent->wait_queue, current_task);

    current_task->exit_status = status;
    current_task->state = TASK_EXITING;
    // Wakeup Neo
    wakeup(&parent->wait_queue);
    switch_locked = false;
    task_switch();
}

void switch_to_user_mode(uint32_t code_addr, uint32_t stack_hi_addr)
{
    current_task->ring = 3;
    switch_to_user_mode_asm(code_addr, stack_hi_addr);
}

