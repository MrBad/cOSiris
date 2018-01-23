#include <string.h>
#include <stdlib.h>
#include "console.h"
#include "task.h"
#include "thread.h"

static phys_t *thread_clone_directory()
{
    uint32_t pd_idx, 
             div4 = 1024 * PAGE_SIZE;
    dir_t *curr_dir = (dir_t *) PDIR_ADDR;
    dir_t *new_dir = (dir_t *) frame_calloc();
    for (pd_idx = 0; pd_idx < 1023; pd_idx++) {
        if (curr_dir[pd_idx] & P_PRESENT) {
            if (pd_idx >= (USER_CODE_START_ADDR/div4)-1 
                && pd_idx < USER_CODE_START_ADDR/div4) 
            {
                clone_pde(new_dir, curr_dir, pd_idx);
                //kprintf("clone %d\n", pd_idx);
            }
            else if (pd_idx >= HEAP_END/div4) {
                clone_pde(new_dir, curr_dir, pd_idx);
                //kprintf("clone %d\n", pd_idx);
            }
            else {
                link_pde(new_dir, curr_dir, pd_idx);
                //kprintf("link %d, %d\n", pd_idx, HEAP_END/div4, KERNEL_STACK_HI/div4);
            }
        }
    }
    recursively_map_page_directory(new_dir);
    return new_dir;
}

static void thread_free_directory(phys_t *dir)
{
    uint32_t pd_idx, 
             div4 = 1024 * PAGE_SIZE;
    for (pd_idx = 0; pd_idx < 1023; pd_idx++) {
        if (pd_idx >= (USER_CODE_START_ADDR/div4)-1 
                && pd_idx < USER_CODE_START_ADDR/div4) 
        {
            free_pde(dir, pd_idx);
            //kprintf("free %d\n", pd_idx);
        }
        else if (pd_idx > HEAP_END/div4) {
            free_pde(dir, pd_idx);
            //kprintf("free %d\n", pd_idx);
        }
    }
    frame_free((phys_t)dir);
}

task_t *thread_new(thread_fn fn, void *arg)
{
    task_t *t;
    int fd;
    uint32_t *u_esp;
    switch_locked = true;
    t = task_new();
    t->is_thread = true;
    t->ppid = current_task->pid;
    t->parent = current_task;
    t->ring = current_task->ring;
    t->uid = current_task->uid;
    t->gid = current_task->gid;
    // link pages from parent //
    t->page_directory = thread_clone_directory();
    t->heap->end_addr = current_task->heap->end_addr;
    t->root_dir = current_task->root_dir;
    t->cwd = strdup(current_task->cwd);
    // link files - TODO: test //
    t->files = (struct file **) calloc(current_task->num_files, sizeof(struct file *));
    for (fd = 0; fd < current_task->num_files; fd++) {
        if (current_task->files[fd]) {
            t->files[fd] = current_task->files[fd];
            t->files[fd]->fs_node = fs_dup(t->files[fd]->fs_node);
        }
    }
    t->num_files = current_task->num_files;
    // Inherit curstom signal handlers or should not?
    //memcpy(t->sig_handlers, current_task->sig_handlers,
    //        NUM_SIGS * sizeof(*current_task->sig_handlers));
 
    t->ebp = get_ebp();
    t->esp = get_esp();
    get_last_task()->next = t;
    t->state = TASK_RUNNING;
    t->eip = get_eip();
    /**
     * Here will jump the new created thread (child), after get_eip().
     * If current task is the new child, aka scheduler switched to it
     * Go to user mode and execute the function
     */
    if (current_task == t) {
        //kprintf("Child pid %d\n", current_task->pid);
        u_esp = (uint32_t *) USER_STACK_HI;
        *--u_esp = *(uint32_t *)arg;
        *--u_esp = 0xC0DEDAC0;
        switch_to_user_mode((uint32_t)fn, (uint32_t)u_esp);
    }
    switch_locked = false;
    return t;
}

void thread_free(task_t *t)
{
    int fd;
    kprintf("Free task %d\n", t->pid);
    switch_locked = true;
    thread_free_directory(t->page_directory);
    list_close(t->wait_queue);
    for (fd = 0; fd < t->num_files; fd++) {
        if (t->files[fd]) {
            fs_close(t->files[fd]->fs_node);
            t->files[fd]->dup_cnt--;
        }
    }
    free(t->files);
    if (t->name)
        free(t->name);
    free(t);
    switch_locked = false;
}

