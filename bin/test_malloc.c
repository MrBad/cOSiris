#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


int main()
{
	unsigned int i;
	char *p;

	free(malloc(1));
	int *k;
	k = (int *)malloc(10000 * sizeof(int));
	printf("Alloc\n");
	for (i = 4000; i < 5000; i++) {
		p = malloc(i);
		memset(p, 'A', i);
		k[i] = (unsigned int )p;
	}
	printf("Freeing\n");
	for (i = 4000; i < 5000; i++) {
		free((void *)k[i]);
	}
	free(k);
	printf("OK\n");
	return 0;
}
