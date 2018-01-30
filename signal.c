#include <string.h>
#include <stdlib.h>
#include "assert.h"
#include "signal.h"
#include "console.h"
#include "task.h"
#include "thread.h"

void sig_ignore(int signum)
{
    kprintf("pid: %d stopped by signal %d\n", current_task->pid, signum);
}

void sig_terminate(int signum)
{
    kprintf("pid: %d terminated by signal %d\n", current_task->pid, signum);
    task_exit(128 + signum);
}

void sig_coredump(int signum)
{
    kprintf("pid: %d terminated by signal %d, coredump\n", 
            current_task->pid, signum);
    task_exit(128 + signum);
}

void sig_stop(int signum)
{
    kprintf("pid: %d stopped by signal %d\n", current_task->pid, signum);
}

void sig_continue(int signum)
{
    kprintf("pid: %d continue by signal %d\n", current_task->pid, signum);
}

sighandler_t sig_default[32] = {
    [SIGHUP] = sig_terminate,
    [SIGINT] = sig_terminate,
    [SIGQUIT] = sig_coredump,
    [SIGILL] = sig_coredump,
    [SIGABRT] = sig_coredump,
    [SIGFPE] = sig_coredump,
    [SIGKILL] = sig_terminate,
    [SIGSEGV] = sig_coredump,
    [SIGTERM] = sig_terminate,
    [SIGCONT] = sig_continue,
    [SIGSTOP] = sig_stop,
    [SIGTSTP] = sig_stop,
};

sighandler_t signal(int signum, sighandler_t handler)
{
    sighandler_t old_handler;

    if (signum < NUM_SIGS) {
        if (signum != SIGKILL && signum != SIGSTOP) {
            old_handler = current_task->sig_handlers[signum];
            current_task->sig_handlers[signum] = handler;
            return old_handler;
        }
    }
    return SIG_ERR;
}

int kill(int pid, int signum)
{
    task_t *t;
    signal_t *sig;
    
    if (!(t = get_task_by_pid(pid)))
        return -1;
    
    if (!(sig = malloc(sizeof(*sig))))
        return -1;

    sig->pid = pid;
    sig->signum = signum;
    return list_add(sigs_queue, sig) ? 0 : -1;
}

void set_default_signals(sighandler_t sig_handlers[NUM_SIGS])
{
    unsigned int i;
    for (i = 0; i < NUM_SIGS; i++)
        sig_handlers[i] = SIG_DFL;
}

/**
 * Here jumps a task on returning from signal handler (via page_fault)
 * This function does not return :)
 */
void signal_handler_return()
{
    memcpy(current_task, current_task->s_task, sizeof(task_t));
    free(current_task->s_task);
    current_task->s_task = 0;
    set_kernel_esp(KERNEL_STACK_HI);
    if (current_task->state == TASK_AWAKEN) {
        current_task->state = TASK_SLEEPING;
        task_switch_next();
    }
    jump_to_current_task();
}

#define FLOOR(num, align) ((num / align) * align)

/**
 * Process signal queue
 */
void process_signals()
{
    node_t *n;
    task_t *t;
    signal_t *sig;
    sighandler_t handler;
    uint32_t signum, k_esp, *u_esp;

    for (n = sigs_queue->head; n; n = n->next) {
        sig = n->data;
        if (current_task->pid != sig->pid) {
            t = get_task_by_pid(sig->pid);
            // Force a sleeping task to wake up and fire a child thread 
            //  next time
            if (t->state == TASK_SLEEPING)
                t->state = TASK_AWAKEN;
        } else {
            signum = sig->signum;
            list_del(sigs_queue, n);
            free(sig);
            handler = current_task->sig_handlers[signum];
            if (handler == SIG_DFL) {
                sig_default[signum](signum);
            } else if (handler != SIG_IGN) {
                /**
                 * Save current task in s_task, create a new kernel stack,
                 *  use user stack to jump to signal handler
                 */
                current_task->s_task = malloc(sizeof(task_t));
                memcpy(current_task->s_task, current_task, sizeof(task_t));
                u_esp = (uint32_t *)*(((uint32_t *)KERNEL_STACK_HI) - 2);
                k_esp = FLOOR(get_esp(), PAGE_SIZE);
                if (k_esp - get_esp() < 0x100)
                    k_esp -= PAGE_SIZE;
                KASSERT((uint32_t)k_esp - get_esp() > 0x100);
                *--u_esp = signum;
                *--u_esp = 0xC0DEDAC0;
                set_kernel_esp(k_esp);
                switch_to_user_mode((uint32_t) handler, (uint32_t) u_esp);
            }
        }
    }
}

