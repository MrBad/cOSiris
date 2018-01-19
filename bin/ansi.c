#include <stdio.h>

int main()
{
    printf("Testing colors\n");
    printf("\033[47m\033[30mblack on white\033[0m\n");
    printf("\033[37m c\033[31m O\033[32m S\033[33m i\033[34m r\033[35m i\033[36m s\033[30m. \033[0m\n");
    printf("\033[30m\033[47m c\033[41m O\033[42m S\033[43m i\033[44m r\033[45m i\033[46m s\033[40m. \033[0m\n");
    printf("\033[37;1m c\033[31;1m O\033[32;1m S\033[33;1m i\033[34;1m r\033[35;1m i\033[36;1m s\033[30;1m. \033[0m\n");
    printf("Test backward\r");
    printf("This should overwrite previous line\n");
    printf("Testing backspaa\bce\n");

    return 0;
}
