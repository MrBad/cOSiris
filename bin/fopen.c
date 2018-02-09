#include <stdio.h>

int main()
{
    char str[512];
    FILE *fp = fopen("README.txt", "r");
    if (!fp)
        return 1;

    char *p = NULL;
    unsigned int n = 0;

    while (getline(&p, &n, fp) > 0)
        printf("%d: %s", n, p);
    
    fclose(fp);
}
