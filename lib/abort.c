#include <signal.h>
#include <unistd.h>

void abort()
{
    kill(getpid(), SIGABRT);
}
