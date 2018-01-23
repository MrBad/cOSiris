#ifndef _LIB_SIGNAL_H
#define _LIB_SIGNAL_H

typedef void (*sighandler_t)(int);

enum {
    SIGHUP  = 1,    // 1    Terminal line hangup
    SIGINT  = 2,    // 2    Interrupt program, CTRL+C
    SIGQUIT = 3,    // 3    Quit program, CTRL+\ //
    SIGILL  = 4,    // 4    Illegal instruction
    SIGABRT = 6,    // 6    Abnormal termination
    SIGFPE  = 8,    // 8    Arithmetic exception, Floating point exception
    SIGKILL = 9,    // 9    Kill(can't be caught or ignored) (POSIX)
    SIGSEGV = 11,   // 11   Invalid memory access
    SIGTERM = 15,   // 15   Software termination signal
    SIGCONT = 18,   // 18   Continue, CTRL+Q
    SIGSTOP = 19,   // 19   Stop executing(can't be caught or ignored) (POSIX)
    SIGTSTP = 20,   // 20   Terminal stop signal (POSIX) - CTRL+Z
};

#define SIG_DFL (sighandler_t) 0
#define SIG_ERR (sighandler_t) -1
#define SIG_IGN (sighandler_t) 1

typedef int sig_atomic_t;

/**
 * Installs a sighandler_t handler for the given signal signum
 * Returns previously installed handler.
 */
sighandler_t signal(int signum, sighandler_t handler);

/**
 * Sends signum to process id pid
 */
int kill(int pid, int signum);

#endif
