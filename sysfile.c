#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "console.h"
#include "task.h"
#include "sysfile.h"
#include "vfs.h"
#include "serial.h"


// this belongs to sys_proc, but... //
int sys_exec(char *path, char *argv[])
{

	fs_node_t *fs_node;
	char **tmp_argv = NULL;
	int argc = 0, i;

	// try to open path //
	if(fs_open_namei(path, O_RDONLY, 0755, &fs_node) < 0) {
		serial_debug("sys_exec() cannot open %s file\n", path);
		return -1;
	}
	if(!(fs_node->flags & FS_FILE)) {
		serial_debug("sys_exec() %s is not a file\n", path);
		return -1;
	}
	// todo-try to see it's mask - not supported yet //

	// save argvs - we will destroy current process stack //
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
	unsigned int num_pages = (fs_node->length / PAGE_SIZE) + 1;
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
		size = fs_read(fs_node, offset, fs_node->length, buff);
		offset += size;
	} while(size > 0);

	// after mapped num_pages, we have some free mem - let's use it for passing arguments //
	// maibe in future we will use stack instead of this, pushing strings into it //
	void *free_mem_start = (void *) buff+offset;
	unsigned int free_mem_size = num_pages*PAGE_SIZE-fs_node->length;
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
	switch_to_user_mode(USER_CODE_START_ADDR, (uint32_t)stack);

	return 0;
}

struct file *alloc_file()
{
	struct file *f;
	f = (struct file *) calloc(1, sizeof(struct file));
	return f;
}

int sys_open(char *filename, int flags, int mode)
{
	// kprintf("opening %s\n", filename);
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
		current_task->num_files = items;
		// kprintf("reallocated to %d files\n", items);
	}

	current_task->files[fd] = malloc(sizeof(struct file));

	if((fs_open_namei(filename, flags, mode, &current_task->files[fd]->fs_node)) < 0) {
		serial_debug("sys_open() cannot open %s\n", filename);
		return -1;
	}
	if(!current_task->files[fd]->fs_node) {
		current_task->files[fd] = NULL;
		return -1;
	}
	current_task->files[fd]->fs_node->ref_count++;
	current_task->files[fd]->offs = 0;
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
	current_task->files[fd]->fs_node->ref_count--;
	// kprintf("pid: %d closing fd: %d, ref_count: %d\n", current_task->pid, fd, current_task->files[fd]->fs_node->ref_count);
	if(current_task->files[fd]->fs_node->ref_count == 0) {
		fs_close(current_task->files[fd]->fs_node);
	}
	free(current_task->files[fd]);
	current_task->files[fd] = NULL;
	return 0;
}

static void populate_stat_buf(fs_node_t *fs_node, struct stat *buf)
{
	buf->st_dev = 0;									// the device id where it reside
	buf->st_ino = fs_node->inode;
	buf->st_mode = fs_node->flags;
	buf->st_nlink = 0;									// not implemented yet -> number of links
	buf->st_uid = fs_node->uid;
	buf->st_gid = fs_node->gid;
	buf->st_rdev = 0;									// the device id of..?!
	buf->st_atime = buf->st_mtime = buf->st_ctime = 0;	// not available yet;
}

int sys_stat(char *path, struct stat *buf)
{
	fs_node_t *fs_node;
	if(fs_open_namei(path, O_RDONLY, 0755, &fs_node) < 0) {
		serial_debug("sys_stat() cannot open %s file\n", path);
		return -1;
	}
	populate_stat_buf(fs_node, buf);
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
		kprintf("File %d not opened by proc %d\n", fd, current_task->files[fd]);
		return -1;
	}
	// EOF ? //
	if(f->offs >= f->fs_node->length && f->fs_node->flags != FS_CHARDEVICE) {
		// serial_debug("eof on file: fd: %d, %s, offs: %d length: %d\n", fd, f->fs_node->name, f->offs, f->fs_node->length);
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
		kprintf("File %d not opened by proc %d\n", fd, current_task->files[fd]);
		return -1;
	}
	unsigned int bytes = fs_write(f->fs_node, f->offs, count, buf);
	f->offs += bytes;
	return bytes;
}

int sys_chdir(char *path)
{
	fs_node_t *fs_node;
	if(!fs_open_namei(path, O_RDONLY, 0777, &fs_node)) {
		serial_debug("sys_chdir() cannot open %s\n", path);
		return -1;
	}
	if(!(fs_node->flags & FS_DIRECTORY)) {
		serial_debug("sys_chdir() - %s is not a directory\n", path);
		return -1;
	}
	current_task->cwd = path;
	return 0;
}

int sys_chroot(char *path)
{
	fs_node_t *fs_node;
	if(!fs_open_namei(path, O_RDONLY, 0777, &fs_node)) {
		serial_debug("sys_chroot() cannot open %s\n", path);
		return -1;
	}
	if(!(fs_node->flags & FS_DIRECTORY)) {
		serial_debug("sys_chroot() - %s is not a directory\n", path);
		return -1;
	}
	// TODO - also check permissions //
	current_task->root_dir = fs_node;
	return 0;
}

// check permissions //
int sys_chmod(char *filename, int uid, int gid)
{
	fs_node_t *fs_node;
	if(!fs_open_namei(filename, O_RDONLY, 0777, &fs_node)) {
		serial_debug("sys_chmod() cannot open %s\n", filename);
		return -1;
	}
	fs_node->uid = uid;
	fs_node->gid = gid;
	return 0;
}

int sys_chown(char *filename, int mode)
{
	fs_node_t *fs_node;
	if(!fs_open_namei(filename, O_RDONLY, 0777, &fs_node)) {
		serial_debug("sys_chown() cannot open %s\n", filename);
		return -1;
	}
	fs_node->mask = mode;
	return 0;
}


int sys_mkdir(char *path, int mode)
{
	int i, len;
	char *tmp, *dirname, *basename;
	if(!path) return -1;

	if(*path != '/') { // relative path
		tmp = calloc(1, strlen(current_task->cwd)+1 + strlen(path)+1);
		strcat(tmp, current_task->cwd);
		strcat(tmp, path);
	} else { 			// absolute path
		tmp = strdup(path);
	}

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
		return -1;
	}
	if(!fs_mkdir(dir, basename, mode)) {
		serial_debug("Cannot create file: %s\n", basename);
		return -1;
	}
	return 0;
}
