#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>
#include <signal.h>

#define NUM_SIGS 32

typedef struct {
    uint32_t signum;  // signal number
    pid_t pid;     // pid of process to apply signal
} signal_t;

void set_default_signals(sighandler_t sig_handlers[NUM_SIGS]);
int signal_is_default(sighandler_t handler, int signum);
void process_signals();

#endif
