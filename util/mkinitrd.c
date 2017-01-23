#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


struct initrd_head {
	unsigned int magic;
	char name[64];
	unsigned int offset;
	unsigned int length;
};

typedef struct initrd_head initrd_head_t;

int main(int argc, char **argv)
{
	if(argc < 2) {
		printf("%s [filenames]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	unsigned int file_offset; int i;
	initrd_head_t headers[64];
	struct stat fstat;
	FILE *fp, *rfp;

	file_offset = sizeof(initrd_head_t) * 64  + sizeof(int);
	printf("h:%lu, off: %d\n", sizeof(initrd_head_t), file_offset);
	for(i = 0; i < argc-1 && i < 65; i++) {
		printf("writing file %s at %x\n", argv[i+1], file_offset);
		strncpy(headers[i].name, basename(argv[i+1]), sizeof(headers[i].name)-1);
		headers[i].offset = file_offset;
		if(stat(argv[i+1], &fstat) < 0) {
			perror("fstat");
			exit(EXIT_FAILURE);
		}
		headers[i].length = fstat.st_size;
		file_offset  +=  fstat.st_size;
		headers[i].magic = 0xdeadbeef;
	}
	fp = fopen("initrd.img", "w");
	if(!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	fwrite(&(i), sizeof(int), 1, fp);
	fwrite(&headers, sizeof(initrd_head_t), 64, fp);

	for (i = 0; i < argc-1 && i < 65; i++) {
		rfp = fopen(argv[i+1], "r");
		if(!rfp) {
			perror("fopen");
			fclose(fp);
			unlink("initrd.img");
		}
		size_t bytes;
		char buff[255];
		do {
			bytes = fread(buff, 1, 255, rfp);
			if(bytes > 0) {
				fwrite(buff, 1, bytes, fp);
			}
		} while(!feof(rfp));
		fclose(rfp);
	}
	fclose(fp);
	return EXIT_SUCCESS;
}
