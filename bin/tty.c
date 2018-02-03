#include <stdio.h>

int main()
{
    printf("\033[1;1H");
    printf("\033[0K");
    printf("\033[1;80H\033[31m+\033[0m");
    printf("\033[24;80H");
    printf("\033[31m+\033[0m");
    printf("[]");
}
