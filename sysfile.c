#include <stdlib.h>
#include "console.h"
#include "task.h"
#include "sysfile.h"


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
		kprintf("reallocated to %d files\n", items);
	}

	current_task->files[fd] = malloc(sizeof(struct file));
	if((fs_open_namei(filename, flags, mode, &current_task->files[fd]->fs_node)) < 0) {
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

int sys_stat(const char *pathname, struct stat *buf){}
int sys_fstat(int fd, struct stat *buf){}

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

int sys_chdir(char *filename){}
int sys_chroot(char *filename){}
int sys_chmod(char *filename, int uid, int gid){}
int sys_chown(char *filename, int mode){}
int sys_mkdir(const char *pathname, int mode){}
