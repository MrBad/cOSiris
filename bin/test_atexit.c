#include <stdio.h>
#include <stdlib.h>

void func1()
{
    printf("This is first function, should be executed second.\n");
}

void func2()
{
    printf("This is second func, should be executed first.\n");
}

int main()
{
    atexit(func1);
    atexit(func2);

    printf("Going to exit, testing exit functions.\n");

    return 0;
}
