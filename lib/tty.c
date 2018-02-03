#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

int tcsetpgrp(int fd, pid_t pid)
{
    if (!isatty(fd))
        return -1;

    return ioctl(fd, TIOCSPGRP, &pid);
}

pid_t tcgetpgrp(int fd)
{
    pid_t pid;

    if (!isatty(fd))
        return -1;
    if (ioctl(fd, TIOCGPGRP, &pid) > 0)
        return pid;
    else
        return -1;
}

int tcgetattr(int fd, struct termios *termios_p)
{
    if (!isatty(fd))
        return -1;
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int opt, const struct termios *termios_p)
{
    (void) opt; // this should be handled - when to set them.
    if (!isatty(fd))
        return -1;
    return ioctl(fd, TCSETS, termios_p);
}

