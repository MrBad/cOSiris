#include <stdlib.h>
#include "assert.h"
#include "signal.h"
#include "console.h"
#include "task.h"
#include "thread.h"

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

// TODO: SIG_DFL should be specific for every signal type, but
// for now we will use only task_exit
void set_default_signals(sighandler_t sig_handlers[NUM_SIGS])
{
    unsigned int i;
    for (i = 0; i < NUM_SIGS; i++)
        sig_handlers[i] = SIG_DFL;
}

void process_signals()
{
    node_t *n;
    task_t *t;
    signal_t *sig;
    sighandler_t handler;
    uint32_t signum;

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
                kprintf("SIGTERM for pid:%d\n", current_task->pid);
                task_exit(128 + signum);
            } else if (handler != SIG_IGN) {
                thread_new((thread_fn)handler, &signum);
                // If this process was forced to wake up, put it back to sleep
                //   and don't let it run!
                if (current_task->state == TASK_AWAKEN) {
                    current_task->state = TASK_SLEEPING;
                    task_switch();
                }
                break;
            }
        }
    }
}
