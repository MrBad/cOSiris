#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include "assert.h"
#include "console.h"
#include "task.h"
#include "sysfile.h"
#include "vfs.h"
#include "serial.h"
#include "canonize.h"


// this belongs to sys_proc, but... //
int sys_exec(char *path, char *argv[])
{

	fs_node_t *fs_node;
	char **tmp_argv = NULL;
	int argc = 0, i;
	// try to open path //
	if(fs_open_namei(path, O_RDONLY, 0, &fs_node) < 0) {
		serial_debug("sys_exec() cannot open %s file\n", path);
		char *p = canonize_path("/", path); // try to open from / root
		if(fs_open_namei(p, O_RDONLY, 0, &fs_node) < 0) {
			free(p);
			return -1;
		}
		free(p);
	}
	if(!(fs_node->type & FS_FILE)) {
		serial_debug("sys_exec() %s is not a file\n", path);
		return -1;
	}
	// TODO try to check it's mask, if it's exec
	// not supported yet //
	
	// save argvs - we will destroy current process stack //
	// TODO - save this args on new stack
	int needed_mem = 0;
	if(argv) {
	 	tmp_argv = calloc(MAX_ARGUMENTS, sizeof(char *));
		for(argc = 0; argc < MAX_ARGUMENTS; argc++) {
			if(!argv[argc]) break;
			tmp_argv[argc] = strdup(argv[argc]);
			needed_mem += strlen(argv[argc]) + sizeof(char) + sizeof(uint32_t);
		}
	}
	// change name //
	if(current_task->name) free(current_task->name);
	current_task->name = strdup(fs_node->name);

	// map if program needs more pages //
	unsigned int num_pages = (fs_node->size / PAGE_SIZE) + 1;
	for(i = 0; i < num_pages; i++) {
		virt_t page = USER_CODE_START_ADDR + PAGE_SIZE * i;
		if(!is_mapped(page)) {
			map(page, (unsigned int)frame_alloc(), P_PRESENT | P_READ_WRITE | P_USER);
		}
	}
	// free it's stack //
	for(i = 2; i > 0; i--) {
		virt_t page = USER_STACK_HI-(i * PAGE_SIZE);
		if(!is_mapped(page)) {
			map(page, (unsigned int)frame_alloc(), P_PRESENT | P_READ_WRITE | P_USER);
		}
		memset((void *)page, 0, PAGE_SIZE);
	}

	// load/overwrite program into memory //
	unsigned int offset = 0, size = 0;
	char *buff = (char *)USER_CODE_START_ADDR;
	do {
		size = fs_read(fs_node, offset, fs_node->size, buff);
		offset += size;
	} while(size > 0);

	// after mapped num_pages, we have some free mem - let's use it for passing arguments //
	// maibe in future we will use stack instead of this, pushing strings into it //
	void *free_mem_start = (void *) (buff+offset);
	unsigned int free_mem_size = num_pages*PAGE_SIZE-fs_node->size;
	// zero the rest of alloc space //
	memset(buff+offset, 0, free_mem_size);
	// kprintf("Free mem starts at: %p, size: %d, needed_mem: %d\n", free_mem_start, free_mem_size, needed_mem);
	if(needed_mem > free_mem_size) {
		kprintf("alloc another free page pls or clean this code!\n");
		return -1;
	}
	// if we have 2 argv for example, we will have a memory like free mem=new_argv | ptr0 | ptr1| ptr1 points here 0| ptr2 points here|
	char *p;
	uint32_t *new_argv = (uint32_t *)free_mem_start;
	p = (char *)(new_argv + argc);
	for(i = 0; i < argc; i++) {
		strcpy(p, tmp_argv[i]);
		new_argv[i] = (uint32_t)p;
		int len = strlen(tmp_argv[i]);
		p[len]=0;
		p+=len;
		free(tmp_argv[i]);
	}
	if(tmp_argv) free(tmp_argv);

	uint32_t *stack = (uint32_t *)(USER_STACK_HI);
	stack--;
	*stack = 0xC0DEBABE; // fake return point
	stack--;
	*stack = (uint32_t)new_argv; // push argv into stack //
	stack--;
	*stack = argc; // push argc into stack
	// kprintf("sys_exec(%s, %d params), stack: %p\n", fs_node->name, argc, stack);
	fs_close(fs_node);
	switch_to_user_mode(USER_CODE_START_ADDR, (uint32_t)stack);

	return 0;
}

static struct file *alloc_file()
{
	struct file *f;
	f = (struct file *) calloc(1, sizeof(struct file));
	return f;
}

// Allocates a file descriptor pointer for current proc //
// if it does not find a free one, enlarge the files pointer buffer //
static int fd_alloc()
{
	int fd; 
	for(fd = 0; fd < current_task->num_files; fd++) {
		if(!current_task->files[fd]) {
			break;
		}
	}
	if(fd == current_task->num_files) {
		// not enough available files, let's enlarge the array //
		int items;
		items = current_task->num_files;
		if(items > MAX_OPEN_FILES) {
			return -1; 
		}
		items = items * 2 >= MAX_OPEN_FILES ? MAX_OPEN_FILES : items * 2;
		current_task->files = realloc(current_task->files, items * sizeof(struct file *));
		for(; current_task->num_files < items; current_task->num_files++) {
			current_task->files[current_task->num_files] = NULL; // and null the reallocated buffer //
		}
		KASSERT(current_task->num_files == items);
	}
	return fd;
}

int sys_open(char *filename, int flags, int mode)
{
	// treat mode here!!! //
	
	
	int fd;
	/*	
	for(fd = 0; fd < current_task->num_files; fd++) {
		if(!current_task->files[fd]) {
			break;
		}
	}
	if(fd == current_task->num_files) {
		// not enough available files, let's enlarge the array //
		int items;
		items = current_task->num_files;
		if(items > MAX_OPEN_FILES) {
			return -1;
		}
		items = items * 2 >= MAX_OPEN_FILES ? MAX_OPEN_FILES : items * 2;
		current_task->files = realloc(current_task->files, items * sizeof(struct file *));

		for(; current_task->num_files < items; current_task->num_files++) {
			current_task->files[current_task->num_files] = NULL; // and null the reallocated buffer //
		}
		current_task->num_files = items;
	}*/
	if((fd = fd_alloc()) < 0) {
		return -1;
	}
	//current_task->files[fd] = malloc(sizeof(struct file));
	current_task->files[fd] = alloc_file();
	
	if((fs_open_namei(filename, flags, mode, &current_task->files[fd]->fs_node)) < 0) {
		serial_debug("sys_open() cannot open %s\n", filename);
		free(current_task->files[fd]);
		current_task->files[fd] = NULL;
		return -1;
	}
	if(!current_task->files[fd]->fs_node) {
		kprintf("should not happen?!?!\n");
		free(current_task->files[fd]);
		current_task->files[fd] = NULL;
		return -1;
	}
	if(flags & O_APPEND && (flags & O_WRONLY || flags & O_RDWR)) {
		current_task->files[fd]->offs = current_task->files[fd]->fs_node->size;
	} else {
		current_task->files[fd]->offs = 0;
	}
	current_task->files[fd]->flags = flags;
	current_task->files[fd]->mode = mode;
	return fd;
}

int sys_close(unsigned int fd)
{
	if(fd > current_task->num_files) {
		return -1;
	}
	if(!current_task->files[fd]) {
		return -1;
	}
	if(current_task->files[fd]->dup_cnt < 0) {
		kprintf("dup close error ?!\n");
		return -1;
	}
	fs_close(current_task->files[fd]->fs_node);
	if(current_task->files[fd]->dup_cnt > 0)
		current_task->files[fd]->dup_cnt--;
	// when dup_cnt is 0, it is safe to free the 
	// structure
	if(current_task->files[fd]->dup_cnt == 0) {
		free(current_task->files[fd]);
		current_task->files[fd] = NULL;
	}
	return 0;
}

static void populate_stat_buf(fs_node_t *fs_node, struct stat *buf)
{
	buf->st_dev = 0;									// the device id where it reside
	buf->st_ino = fs_node->inode;
	buf->st_size = fs_node->size;
	buf->st_mode = fs_node->type;
	buf->st_nlink = fs_node->num_links;					// not implemented yet -> number of links
	buf->st_uid = fs_node->uid;
	buf->st_gid = fs_node->gid;
	buf->st_rdev = 0;									// the device id of..?!
	buf->st_atime = fs_node->atime;
	buf->st_mtime = buf->st_mtime;
	buf->st_ctime = buf->st_ctime;
}

// fixme links //
int sys_stat(char *path, struct stat *buf)
{
	fs_node_t *fs_node;
	if(fs_open_namei(path, O_RDONLY, 0755, &fs_node) < 0) {
		serial_debug("sys_stat() cannot open %s file\n", path);
		return -1;
	}
	populate_stat_buf(fs_node, buf);
	fs_close(fs_node);
	return 0;
}
// fix me links //
int sys_lstat(char *path, struct stat *buf) {
	fs_node_t *fs_node;
	if(fs_open_namei(path, O_RDONLY, 0755, &fs_node) < 0) {
		serial_debug("sys_stat() cannot open %s file\n", path);
		return -1;
	}
	populate_stat_buf(fs_node, buf);
	fs_close(fs_node);
	return 0;
}
int sys_fstat(int fd, struct stat *buf)
{
	struct file *f;
	if(fd > current_task->num_files) {
		return -1;
	}
	if(!current_task->files[fd]) {
		return -1;
	}
	f = current_task->files[fd];
	populate_stat_buf(f->fs_node, buf);
	return 0;
}

int sys_read(int fd, void *buf, size_t count)
{
	struct file *f;
	if(fd > current_task->num_files) {
		kprintf("pid: %d, fd %d is bigger than allocated: %d\n",current_task->pid, fd, current_task->num_files);
		return -1;
	}
	// File was not previously open //
	if(!(f = current_task->files[fd])) {
		kprintf("File %d not opened by proc %d\n", fd, current_task->pid);
		return -1;
	}
	// EOF ? //
	if(f->offs >= f->fs_node->size && f->fs_node->type != FS_CHARDEVICE) {
		// serial_debug("eof on file: fd: %d, %s, offs: %d size: %d\n", fd, f->fs_node->name, f->offs, f->fs_node->size);
		return 0; // at EOF
	}
	unsigned int bytes = fs_read(f->fs_node, f->offs, count, buf);
	f->offs += bytes;
	return bytes;
}

int sys_write(int fd, void *buf, size_t count)
{
	struct file *f;
	if(fd > current_task->num_files) {
		kprintf("pid: %d, fd %d is bigger than allocated: %d\n",current_task->pid, fd, current_task->num_files);
		return -1;
	}
	// File was not previously open //
	if(!(f = current_task->files[fd])) {
		kprintf("File %d not opened by proc %d\n", fd, current_task->pid);
		return -1;
	}
	unsigned int bytes = fs_write(f->fs_node, f->offs, count, buf);
	f->offs += bytes;
	return bytes;
}
int write(int fd, void *buf, size_t count) {
	return sys_write(fd, buf, count);
}

int sys_chdir(char *path)
{

	//char *p = canonize_path(current_task->cwd, path);
	char *p;
	/*if(path[0]=='/')
		p = strdup(path);
	else {
		p = malloc(strlen(current_task->cwd)+strlen(path)+1+1);
		strcpy(p,current_task->cwd);
		strcat(p,"/");
		strcat(p,path);
	}*/
	p = canonize_path(current_task->cwd, path);
	fs_node_t *fs_node;
	if((fs_open_namei(p, O_RDONLY, 0777, &fs_node)) < 0) {
		kprintf("sys_chdir() cannot open %s\n", path);
		free(p);
		return -1;
	}
	if(!(fs_node->type & FS_DIRECTORY)) {
		kprintf("sys_chdir() - %s is not a directory\n", path);
		free(p);
		fs_close(fs_node);
		return -1;
	}
	//kprintf("chdir %s, %s, ref_c: %d\n", p, fs_node->name, fs_node->ref_count);
	free(current_task->cwd);
	current_task->cwd = p;
	fs_close(fs_node);
	return 0;
}

int sys_chroot(char *path)
{
	fs_node_t *fs_node;
	if((fs_open_namei(path, O_RDONLY, 0777, &fs_node)) < 0) {
		serial_debug("sys_chroot() cannot open %s\n", path);
		return -1;
	}
	if(!(fs_node->type & FS_DIRECTORY)) {
		serial_debug("sys_chroot() - %s is not a directory\n", path);
		fs_close(fs_node);
		return -1;
	}
	// TODO - also check permissions //
	current_task->root_dir = fs_node;
	return 0;
}

// check permissions //
int sys_chown(char *filename, int uid, int gid)
{
	fs_node_t *fs_node;
	if((fs_open_namei(filename, O_RDONLY, 0777, &fs_node)) < 0) {
		serial_debug("sys_chmod() cannot open %s\n", filename);
		return -1;
	}
	fs_node->uid = uid;
	fs_node->gid = gid;
	fs_close(fs_node);
	return 0;
}

int sys_chmod(char *filename, int mode)
{
	fs_node_t *fs_node;
	if((fs_open_namei(filename, O_RDONLY, 0777, &fs_node)) < 0) {
		serial_debug("sys_chown() cannot open %s\n", filename);
		return -1;
	}
	fs_node->mask = mode;
	fs_close(fs_node);
	return 0;
}


int sys_mkdir(char *path, int mode)
{
	int i, len;
	char *tmp, *dirname, *basename;
	if(!path) return -1;
	if(*path != '/') { // relative path
		tmp = calloc(1, strlen(current_task->cwd) + strlen(path) + 4);
		strcat(tmp, current_task->cwd);
		strcat(tmp, "/");
		strcat(tmp, path);
	} else {			// absolute path
		tmp = strdup(path);
	}

	//tmp = canonize_path(current_task->cwd, path);
	while(tmp && tmp[strlen(tmp)-1] == '/') tmp[strlen(tmp)-1] = 0;
	len = strlen(tmp);
	for(i = len; i > 0; i--)
		if(tmp[i] == '/')
			break;
	if(i == len) return -1;

	dirname = strdup(tmp); dirname[i] = 0;
	basename = strdup(tmp); memmove(basename, basename+i+1, len-i); basename[len-i]=0;
	fs_node_t *dir;

	if(fs_open_namei(dirname, O_RDONLY, 0777, &dir)<0) {
		serial_debug("Cannot open dirname %s\n", dirname);
		return -1;
	}
	fs_node_t *file;
	if((file = fs_finddir(dir, basename))) {
		serial_debug("Dir exists %s\n", file->name);
		fs_close(dir);
		fs_close(file);
		return -1;
	}
	if(!(file = fs_mkdir(dir, basename, mode))) {
		serial_debug("Cannot create file: %s\n", basename);
		fs_close(dir);
		return -1;
	}
	fs_close(file);
	fs_close(dir);
	free(dirname);
	free(basename);
	free(tmp);
	return 0;
}

int sys_isatty(int fd)
{
	struct file *f;
	if(fd > current_task->num_files) {
		kprintf("pid: %d, fd %d is bigger than allocated: %d\n",current_task->pid, fd, current_task->num_files);
		return 0;
	}
	// File was not previously open //
	if(!(f = current_task->files[fd])) {
		kprintf("File %d not opened by proc %d\n", fd, current_task->files[fd]);
		return 0;
	}
	if(f->fs_node && FS_CHARDEVICE) {
		return 1;
	}
	return 0;
}

off_t sys_lseek(int fd, off_t offset, int whence)
{
	struct file *f;
	if(fd > current_task->num_files) {
		kprintf("pid: %d, fd %d is bigger than allocated: %d\n",current_task->pid, fd, current_task->num_files);
		return -1;
	}
	// File was not previously open //
	if(!(f = current_task->files[fd])) {
		kprintf("File %d not opened by proc %d\n", fd, current_task->files[fd]);
		return -1;
	}
	if(whence == SEEK_SET) {
		if(offset > f->fs_node->size) {
			kprintf("sys_lseek() offs > size\n");
			return -1;
		}
		f->offs = offset;
	} else if(whence == SEEK_CUR) {
		if(offset + f->offs > f->fs_node->size) {
			kprintf("sys_lseek() offs + f->offs > fs_node->size\n");
			return -1;
		}
		f->offs += offset;
	} else if(whence == SEEK_END) {
		f->offs = offset;
	}
	return f->offs;
}

DIR *sys_opendir(char *dirname)
{
	DIR *d;
	int fd = sys_open(dirname, O_RDONLY, 0);
	if(!fd) {
		return NULL;
	}
	d = current_task->files[fd];
	if(!(d->fs_node->type & FS_DIRECTORY)) {
		sys_close(fd);
		return NULL;
	}
	return d;
}

int sys_closedir(DIR *dir)
{
	int fd;
	for(fd = 0; fd < current_task->num_files; fd++) {
		if(dir == current_task->files[fd]) {
			break;
		}
	}
	if(fd < current_task->num_files) {
		sys_close(fd);
		return 0;
	} else {
		return -1;
	}
}

// i cannot return in user mode a buffer from kernel mode
// that's why i will pass buf from user mode and populate in k mode
int sys_readdir(DIR *dir, struct dirent *buf)
{
	struct dirent *d;
	if(!(d = fs_readdir(dir->fs_node, dir->offs))) {
		return -1;
	}
	strncpy(buf->d_name, d->d_name, sizeof(buf->d_name)-1);
	buf->d_ino = d->d_ino;
	dir->offs++;
	return 0;
}

int sys_readlink(const char *pathname, char *buf, size_t bufsiz) {
	kprintf("unimplemented\n");
	return -1;
}
/*int readdir_r(DIR *, struct dirent *, struct dirent **);
void rewinddir(DIR *);
void seekdir(DIR *, long int);
long int telldir(DIR *);
*/

char *sys_getcwd(char *buf, size_t size) 
{
	unsigned int len = strlen(current_task->cwd);
	if(len+1 > size) {
		kprintf("len: %d, size %d\n", len, size);
		return NULL;
	}
	strncpy(buf, current_task->cwd, len);
	buf[len]=0;
	return buf;
}

int sys_unlink(char *path) 
{
	fs_node_t *node;
	if((fs_open_namei(path, O_RDONLY, 0, &node)) < 0) {
		serial_debug("sys_unlink() cannot open %s\n", path);
		return -1;
	}
	fs_close(node);
	return fs_unlink(path);
}


int sys_dup(int oldfd) 
{
	int fd, valid;
	valid = 0;
	for(fd = 0; fd < current_task->num_files; fd++) {
		if(fd == oldfd && current_task->files[fd]) {
			valid = 1; break;
		}
	}
	if(!valid) {
		kprintf("sys_dup: invalid file descriptor %d!", oldfd);
		return -1;
	}	
	if((fd = fd_alloc()) < 0) {
		return -1;
	}
	// and we link the files[oldfd] to new files[fd]
	// increasing dup_cnt
	current_task->files[fd] = current_task->files[oldfd];
	fs_dup(current_task->files[fd]->fs_node);
	current_task->files[fd]->dup_cnt++;
	return fd;
}
