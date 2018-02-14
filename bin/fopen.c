#include <stdio.h>

int main()
{
    FILE *fp = fopen("README.txt", "r");
    if (!fp)
        return 1;

    char *p = NULL;
    unsigned int n = 0;

    while (getline(&p, &n, fp) > 0)
        printf("%d: %s", n, p);
    
    fclose(fp);
}
