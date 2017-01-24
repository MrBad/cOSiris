// Prints the symbol located at hex addr using kernel.sym //
// used for debugging purpose //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	unsigned int addr;
	char buff[256];
	if(argc < 2) {
		printf("Usage: %s [0x01234567]\n", argv[0]);
		return 1;
	}
	if(strncmp(argv[1], "0x", 2)) {
		strcpy(buff, "0x");
		strncat(buff, argv[1], 253);
		addr = strtoul(buff, NULL, 0);
	} else {
		addr = strtoul(argv[1], NULL, 0);
	}
	if(addr == 0) {
		perror("strtoul");
		return 1;
	}

	char *file = "../kernel.sym";
	FILE *fp = fopen(file, "r");
	if(!fp) {
		perror("file");
		return 1;
	}

	unsigned int prev_addr = 0, this_addr = 0;
	char address[32]; char type[32]; char func_name[64], prev_func_name[64];
	int found = 0;
	do {
		if(fscanf(fp, "%s %s %s\n", address, type, func_name)){
			char temp[512] = {'0','x'};
			strcat(temp, address);
			this_addr = strtoul(temp, NULL, 0);
			if(addr >= prev_addr && addr < this_addr) {
				printf("Found symbol: %s at address 0x%08x matching 0x%08x\n", prev_func_name, prev_addr, addr);
				found = 1;
				break;
			} else {
				prev_addr = this_addr;
				strcpy(prev_func_name, func_name);
				prev_func_name[64] = 0;
			}
		} else {
			break;
		}
	} while(!feof(fp));

	if(!found) {
		printf("Cannot find any symbol in kernel at address: 0x%08X\n", addr);
	}
	fclose(fp);
	return 0;
}
