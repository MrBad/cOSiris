#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "console.h"
#include "assert.h"
#include "gdt.h"
#include "vfs.h"
#include "pipe.h"
#include "thread.h"
#include "timer.h"
#include "task.h"
#include "dev.h"

char *task_states[] = {
    "TASK_CREATING",
    "TASK_RUNNING",
    "TASK_AWAKEN",
    "TASK_SLEEPING",
    "TASK_EXITING",
    0
};

/**
 * Creates a new task structure
 */
task_t *task_new()
{
    task_t *t = calloc(1, sizeof(task_t));
    t->pid = next_pid++;
    t->wait_queue = list_open(NULL);
    // install default signals
    set_default_signals(t->sig_handlers);
    // heap //
    heap_t *h = calloc(1, sizeof(heap_t));
    h->start_addr = h->end_addr = UHEAP_START;
    h->max_addr = UHEAP_END;
    h->readonly = h->supervisor = false;
    t->heap = h;
    t->name = NULL;

    return t;
}

void set_kernel_esp(uint32_t esp)
{
    write_tss(5, 0x10, esp);
    gdt_flush();
}

/**
 * Initialize multitasking
 */
void task_init()
{
    int i;
    struct file *f;
    cli();
    switch_locked = true;
    next_pid = 1;
    set_kernel_esp(KERNEL_STACK_HI);
    gdt_flush();
    tss_flush();

    current_task = task_new();
    current_task->uid = current_task->gid = 0;
    current_task->page_directory = (dir_t *) virt_to_phys(PDIR_ADDR);
    current_task->root_dir = fs_root;
    current_task->cwd = strdup("/");
    current_task->name = strdup("init");
    current_task->state = TASK_RUNNING;
    task_queue = current_task;
    // create list of queued signals
    sigs_queue = list_open(NULL);
    // alloc files for first process //
    current_task->files = (struct file **) calloc(TASK_INITIAL_NUM_FILES, 
            sizeof(struct file *));
    current_task->num_files = TASK_INITIAL_NUM_FILES;
    for (i = 0; i < 3; i++) {
        f = calloc(1, sizeof(*f));
        f->fs_node = fs_namei("/dev/tty0");
        f->mode = (i == 0 ? O_RDONLY : O_WRONLY);
        current_task->files[i] = f;
        dev_open(f->fs_node, f->mode);
    }
    switch_locked = false;
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
    char buf[512];
    while (t) {
        snprintf(buf, sizeof(buf), "%10s, pid: %2d, ppid: %2d, state: %13s, ssid: %d, pgrp: %d\n",
                t->name ? t->name : "[unnamed]", t->pid, t->ppid, 
                task_states[t->state], t->sid, t->pgrp);
        if (current_task->files[1])
            fs_write(current_task->files[1]->fs_node, 0, strlen(buf), buf);
        else
            kprintf("%s", buf);
        t = t->next;
    }
}

pid_t getpid()
{
    return current_task ? current_task->pid : -1;
}

int getring()
{
    if (!current_task)
        return -1;
    return current_task->regs.cs & 0x3;
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

/**
 * This is the idle CPU loop.
 * An idle process will stay here, in TASK_RUNNING state, and it
 *  will be picked up by the scheduler when no other process can be run.
 * This just execute hlt command, which pause the CPU (reducing power
 * consumption) until a new interrupt arrive.
 */
void idle_task()
{
    free(current_task->name);
    current_task->name = strdup("idle");

    for (;;) {
        sti();
        hlt();
    }
}

int wake_check(task_t *t)
{
    if (t->state == TASK_SLEEPING && t->sleep_end_tick > 0) {
        if (t->sleep_end_tick < timer_ticks) {
            t->state = TASK_RUNNING;
            t->sleep_end_tick = 0;
            return 0;
        }
    }
    return -1;
}

task_t *task_switch_inner()
{
    task_t *n;

    if (switch_locked) {
        return current_task;
    }
    for (n = current_task->next; n; n = n->next) {
        wake_check(n);
        if (n->state == TASK_RUNNING || n->state == TASK_AWAKEN)
            break;
    }
    if (!n) {
        for (n = task_queue; n; n = n->next) {
            wake_check(n);
            if (n->state == TASK_RUNNING || n->state == TASK_AWAKEN)
                break;
        }
    }
    if (!n) {
        panic("All threads sleeping, even idle\n");
    }
    current_task = n;
    return current_task;
}

/**
 * Fork. Must be called from an interrupt, in order for
 * registers to be properly saved.
 */
pid_t fork_int()
{
    int fd;
    cli();
    switch_locked = true;
    task_t *t = task_new();
    t->ppid = current_task->pid;
    t->parent = current_task;
    t->ring = current_task->ring;
    t->uid = current_task->uid;
    t->gid = current_task->gid;
    t->sid = current_task->sid;
    t->pgrp = current_task->pgrp;
    t->page_directory = clone_directory();
    memcpy(&t->regs, &current_task->regs, sizeof(t->regs));

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
        memcpy(t->files[fd], current_task->files[fd], sizeof(struct file));
        fs_dup(current_task->files[fd]->fs_node);

        if (t->files[fd]->fs_node->type & FS_PIPE) {
            vfs_pipe_t *pipe = (vfs_pipe_t*) t->files[fd]->fs_node->ptr;
            if (t->files[fd]->fs_node->flags == O_RDONLY) {
                pipe->readers++;
            } else {
                pipe->writers++;
            }
        }
    }
    t->num_files = current_task->num_files;
    /* Inherit curstom signal handlers */
    memcpy(t->sig_handlers, current_task->sig_handlers,
            NUM_SIGS * sizeof(*current_task->sig_handlers));
    /* Link new task, and let the scheduler do the rest of the job */
    t->state = TASK_RUNNING;
    get_last_task()->next = t;
    /* The child should return 0 */
    t->regs.eax = 0;
    switch_locked = false;
    /* Parent return the pid*/
    return t->pid;
}

void task_switch_int()
{
    current_task = task_switch_inner();
    switch_context();
    process_signals();
    int_return(&current_task->regs);
}

/**
 * Free task alocated memory
 */
void task_free(task_t *task)
{
    int fd;

    KASSERT(task);
    KASSERT(task->state == TASK_EXITING);
    KASSERT(task->wait_queue->num_items == 0);
    KASSERT(task->is_thread == 0);

    switch_locked = true; 
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
    switch_locked = false;
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
 * Sleeps on addr, until it is waked up by other process, or ms has passed
 * Note: this has a timer tick resolution (10ms) and it's not very accurate,
 *  but should be enough. Another solution is to use a separate, decrementing,
 *  timer channel and create a queue of waking up processes.
 */
void sleep_onms(void *addr, uint32_t ms)
{
    uint32_t ticks = ms / MS_PER_TICK;
    if (ms % MS_PER_TICK)
        ticks++;
    current_task->sleep_end_tick = timer_ticks + ticks;
    sleep_on(addr);
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
            cnt++; t->state = TASK_RUNNING;
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
    // wakeup too early
    if (current_task->wait_queue->num_items == 0) {
        switch_locked = false;
        return -2;
    }
    n = current_task->wait_queue->head;
    t = (task_t *) n->data;
    pid = t->pid;
    if (status) 
        *status = t->exit_status;
    list_del(current_task->wait_queue, n);
    if (t->is_thread)
        thread_free(t);
    else
        task_free(t);
    switch_locked = false;
 
    return pid;
}

/**
 * When a task exits, it's resources are not freed until it's parent exits.
 *  1. Reparent running childs.
 *  2. Remove ourself from running queue.
 *  3. Free exited childs from wait_queue.
 *  4. Add ourself to parent waiting queue or 1 if orphan.
 *  5. Delete any existing signal for us?
 */
void task_exit(int status)
{
    task_t *t, *parent;
    node_t *n;
    signal_t *sig;
    switch_locked = true;
    cli();
    if (current_task->pid == 1) {
        kprintf("INIT respawn not available yet.\n");
        return;
    }
    for (t = task_queue; t; t = t->next) {
        if (t->pid == current_task->ppid)
            parent = t; // got parent
        if (current_task->pid == t->ppid) {
            t->ppid = 1;  // my children still in queue, reparent to 1
        }
        if (current_task == t->next)
            t->next = current_task->next; // remove myself from list
    }
    for (n = current_task->wait_queue->head; n; n = n->next) {
        t = n->data;
        if (t->is_thread)
            thread_free(t);
        else
            task_free(t);
        list_del(current_task->wait_queue, n);
    }
    for (n = sigs_queue->head; n; n = n->next) {
        sig = n->data;
        if (sig->pid == current_task->pid) {
            list_del(sigs_queue, n);
            free(sig);
        }
    }
    if (!parent)
        parent = get_task_by_pid(1);
    current_task->exit_status = status;
    current_task->state = TASK_EXITING;
    list_add(parent->wait_queue, current_task);
    if (!current_task->is_thread)
        wakeup(&parent->wait_queue);
    switch_locked = false;
    task_switch();
}

void switch_to_user_mode(uint32_t code_addr, uint32_t stack_hi_addr)
{
    current_task->ring = 3;
    switch_to_user_mode_asm(code_addr, stack_hi_addr);
}

