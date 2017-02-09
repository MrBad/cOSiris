//
//	Creates the file system
//		Inspired from Unix V6 and xv6 reimplementation
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>


#include "cofs.h"
#include "vfs.h"

#define BLOCK_SIZE 512


int fsfd;
struct cofs_superblock superb;
char buf[BLOCK_SIZE];
unsigned int free_inode = 1;
unsigned int free_block = 0;

void write_sector(unsigned int sector, void *buf)
{
	if(lseek(fsfd, sector * BLOCK_SIZE, 0) < 0) {
		perror("lseek");
		exit(1);
	}
	if(write(fsfd, buf, BLOCK_SIZE)!=BLOCK_SIZE) {
		perror("write");
		exit(1);
	}
}

void read_sector(unsigned int sector, void *buf)
{
	if(lseek(fsfd, sector * BLOCK_SIZE, 0) < 0) {
		perror("lseek");
		exit(1);
	}
	if(read(fsfd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
		printf("ERROR on sector %d\n", sector);
		perror("read_sector");
		exit(1);
	}
}

void write_inode(unsigned int inum, cofs_inode_t *ino)
{
	char buffer[BLOCK_SIZE];
	unsigned int sec = INO_BLK(inum, superb); // block which contains this inum
	read_sector(sec, buffer);
	cofs_inode_t *dino = ((cofs_inode_t*)buffer)+(inum % NUM_INOPB);
	*dino = *ino; //
	write_sector(sec, buffer);
}
void read_inode(unsigned int inum, cofs_inode_t *ino)
{
	char buffer[BLOCK_SIZE];
	unsigned int sec = INO_BLK(inum, superb);
	read_sector(sec, buffer);
	cofs_inode_t *dino = ((cofs_inode_t *)buffer)+(inum % NUM_INOPB);
	*ino = *dino;
}

unsigned int inode_alloc(unsigned short int type)
{
	unsigned int inum = free_inode++;
	struct cofs_inode ino;
	memset(&ino, 0, sizeof(ino));
	ino.type = type;
	ino.num_links = 1;
	ino.size = 0;
	write_inode(inum, &ino);
	return inum;
}

void block_alloc(int used)
{
	char buffer[BLOCK_SIZE];
	int i;
	if(used > BLOCK_SIZE * 8) {
		printf("USED > BLOCK_SIZE\n" );
		exit(1);
	}
	memset(&buffer, 0, BLOCK_SIZE);
	for(i=0; i<used; i++) {
		buffer[i/8] = buffer[i/8] | (0x1 <<(i%8));
	}
	write_sector(superb.bitmap_start, buffer);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

// appends from ptr, size bytes int inode number inum
void inode_append(unsigned int inum, void *ptr, unsigned int size)
{
	char *p = (char *)ptr;
	char buf[BLOCK_SIZE];
	struct cofs_inode ino;
	unsigned int sind_buf[BLOCK_SIZE/sizeof(unsigned int)]; // single indirect buffer
	unsigned int dind_buf[BLOCK_SIZE/sizeof(unsigned int)]; // single indirect buffer
	unsigned int offset, file_block_number, csect, n;
	read_inode(inum, &ino);
	offset = ino.size;
	while(size > 0) {
		file_block_number = offset / BLOCK_SIZE;
		if(file_block_number > MAX_FILE_SIZE) {
			printf("File too large > %lu blocks\n", MAX_FILE_SIZE);
			exit(1);
		}
		// direct //
		if (file_block_number < NUM_DIRECT) {
			if(ino.addrs[file_block_number] == 0) {
				ino.addrs[file_block_number] = free_block++;
			}
			csect = ino.addrs[file_block_number];
		}
		// single indirect //
		else if(file_block_number < NUM_DIRECT+NUM_SIND){
			if(ino.addrs[SIND_IDX] == 0) { // alloc a block for single indirect
				ino.addrs[SIND_IDX] = free_block++;
			}
			read_sector(ino.addrs[SIND_IDX], (char *) sind_buf);
			if(sind_buf[file_block_number - NUM_DIRECT] == 0) {
				sind_buf[file_block_number - NUM_DIRECT] = free_block++;
				write_sector(ino.addrs[SIND_IDX], (char *)sind_buf);
			}
			csect = sind_buf[file_block_number - NUM_DIRECT];
		}
		// double indirect //
		else {
			if(ino.addrs[DIND_IDX] == 0) { // alloc a block for double indirect
				ino.addrs[DIND_IDX] = free_block++;
			}
			int midx = (file_block_number-NUM_DIRECT-NUM_SIND) / NUM_SIND;
			int sidx = (file_block_number-NUM_DIRECT-NUM_SIND) % NUM_SIND;
			read_sector(ino.addrs[DIND_IDX], (char *)sind_buf);
			if(sind_buf[midx] == 0) { // alloc 1'st level block
				sind_buf[midx] = free_block++;
				write_sector(ino.addrs[DIND_IDX], (char *)sind_buf);
			}
			read_sector(sind_buf[midx], (char *)dind_buf);
			if(dind_buf[sidx] == 0) { // alloc 2'nd level block
				dind_buf[sidx] = free_block++;
				write_sector(sind_buf[midx], (char *)dind_buf);
			}
			csect = dind_buf[sidx];
		}
		n = min(size, (file_block_number+1) * BLOCK_SIZE - offset);
		read_sector(csect, buf);
		bcopy(p, buf + offset-(file_block_number * BLOCK_SIZE), n);
		write_sector(csect, buf);
		size -= n;
		offset += n;
		p += n;
	}
	ino.size = offset;
	write_inode(inum, &ino);
}

int main(int argc, char *argv[])
{
	if(argc < 3) {
		printf("usage: %s hdd.img files...\n", argv[0]);
		return 1;
	}
	printf("Max file size: %lu\n", MAX_FILE_SIZE * BLOCK_SIZE);
	struct stat fstat;
	unsigned int cofs_size;		// total fs size in blocks
	unsigned int bitmap_size;	// free bitmap size in blocks
	unsigned int inodes_size;	// size of inodes in blocks
	unsigned int num_inodes;

	unsigned int num_meta_blocks;
	unsigned int num_data_blocks;

	if(stat(argv[1], &fstat) < 0){
		printf("Cannot stat %s\n", argv[1]);
		return 1;
	}

	cofs_size = fstat.st_size / BLOCK_SIZE;
	num_inodes = cofs_size * BLOCK_SIZE / 4096; // let's assume one inode for every 4096 bytes...
	bitmap_size = cofs_size / (BLOCK_SIZE * 8) + 1;
	inodes_size = num_inodes / NUM_INOPB + 1;

	if(sizeof(int) != 4) {
		printf("Sizeof int should be 4, got %lu\n", sizeof(int));
		return 1;
	}
	if(BLOCK_SIZE % sizeof(cofs_inode_t) != 0) {
		printf("block size is not multiple of inode size\n");
		return 1;
	}
	if(BLOCK_SIZE % sizeof(cofs_dirent_t) != 0) {
		printf("block size is not multiple of dirent size\n");
		return 1;
	}

	fsfd = open(argv[1], O_RDWR, 0666);
	if(fsfd < 0) {
		perror("open");
		return 1;
	}

	// 1'st block unused, 2'nd block superblock //
	num_meta_blocks = 2 + inodes_size + bitmap_size;
	num_data_blocks = cofs_size - num_meta_blocks;

	superb.magic = COFS_MAGIC;
	superb.size = cofs_size;
	superb.num_blocks = num_data_blocks;
	superb.num_inodes = num_inodes;
	superb.bitmap_start = 2;
	superb.inode_start = 2 + bitmap_size;
	free_block = num_meta_blocks;

	printf("size: %d, data_blocks: %d, num_inodes: %d, bitmap_start: %d, inode_start: %d, num_meta_blocks: %d\n",
		superb.size, superb.num_blocks, superb.num_inodes, superb.bitmap_start, superb.inode_start, num_meta_blocks);

	unsigned int i;
	// bzero fs //
	memset(buf, 0, BLOCK_SIZE);
	for(i=0; i < superb.size; i++) {
		write_sector(i, buf);
	}
	// write superblock //
	memcpy(buf, (void *)&superb, BLOCK_SIZE);
	write_sector(1, buf);

	// root inode //
	int root_inode = inode_alloc(FS_DIRECTORY);
	if(root_inode != 1) {
		printf("Invalid root inode - expected 1\n");
		exit(1);
	}
	struct cofs_dirent dir;
	memset(&dir, 0, sizeof(struct cofs_dirent));
	dir.inode = root_inode;
	strcpy(dir.name, ".");
	inode_append(root_inode, &dir, sizeof(dir));

	memset(&dir, 0, sizeof(struct cofs_dirent));
	dir.inode = root_inode;
	strcpy(dir.name, "..");
	inode_append(root_inode, &dir, sizeof(dir));

	int fd, inode_num, num_bytes;
	for(i = 2; i < (unsigned int)argc; i++) {
		if((fd = open(argv[i], O_RDONLY)) < 0) {
			perror(argv[i]);
			exit(1);
		}
		inode_num = inode_alloc(FS_FILE);
		memset(&dir, 0, sizeof(dir));
		dir.inode = inode_num;
		strncpy(dir.name, basename(argv[i]), DIRSIZE);
		inode_append(root_inode, &dir, sizeof(dir));

		while ((num_bytes = read(fd, buf, sizeof(buf))) > 0) {
			inode_append(inode_num, buf, num_bytes);
		}
		close(fd);
	}

	struct cofs_inode ino;
	read_inode(root_inode, &ino);

	int offset = ino.size;
	offset = ((offset/BLOCK_SIZE)+1) * BLOCK_SIZE;
	ino.size = offset;
	write_inode(root_inode, &ino);
	block_alloc(free_block);

	close(fsfd);

	return 0;
}
