#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "syscalls.h"

int main(int argc, char *argv[])
{
    int fd;
    unsigned int len = 0;
    struct stat st;

    if (argc < 2) {
        printf("%s: file\n", argv[0]);
        printf("%s: file size\n", argv[0]);
        return 1;
    }
    if (argc > 2) {
        len = atoi(argv[2]);
    }
    if ((fd = open(argv[1], O_RDWR | O_CREAT, 0)) < 0) {
        printf("Error opening %s.\n", argv[1]);
        return 1;
    }
    if (ftruncate(fd, len) < 0) {
        printf("Cannot truncate %s.\n", argv[1]);
        close(fd);
        return 1;
    }
    if(fstat(fd, &st) < 0) {
        printf("Cannot fstat %s.\n", argv[1]);
        close(fd);
        return 1;
    }
    if (st.st_size != len) {
        printf("Error in truncating %s. Expecting %d but got %d\n",
                len, st.st_size);
        close(fd);
        return 1;
    }
    printf("Truncated %s to %d bytes\n", argv[1], st.st_size);
    close(fd);

    return 0;
}

