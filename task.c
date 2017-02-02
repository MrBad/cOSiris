#include <sys/types.h>
#include <stdlib.h>
#include "x86.h"
#include "task.h"
#include "mem.h"
#include "isr.h"
#include "console.h"
#include "gdt.h"
#include "assert.h"
#include "vfs.h"

bool switch_locked = false;
char *task_states[] = {
	"TASK_CREATING",
	"TASK_READY",
	"TASK_SLEEPING",
	"TASK_EXITING",
	"TASK_EXITED",
	0
};

void print_int(int x)
{
	kprintf("%08x", x);
}

task_t *task_new()
{
	task_t *t = (task_t *) calloc(1, sizeof(task_t));
	t->pid = next_pid++;
	// t->tss_kernel_stack = (unsigned int *)KERNEL_STACK_HI;//malloc_page_aligned(PAGE_SIZE);
	t->wait_queue = list_open(NULL);
	// heap //
	heap_t *h  = calloc(1, sizeof(heap_t));
	h->start_addr = h->end_addr = UHEAP_START;
	h->max_addr = UHEAP_END;
	h->readonly = h->supervisor = false;
	t->heap = h;
	// files //
	t->files = (struct file **) calloc(TASK_INITIAL_NUM_FILES, sizeof(struct file *));
	t->num_files = TASK_INITIAL_NUM_FILES;
	// t->files[0] = malloc(sizeof(struct file));
	// t->files[0]->fs_node = STDIN;
	// t->files[1] = malloc(sizeof(struct file));
	// t->files[1]->fs_node = STDOUT;
	// t->files[2] = malloc(sizeof(struct file));

	return t;
}

void task_init()
{
	cli();
	next_pid = 1;
	write_tss(5, 0x10, KERNEL_STACK_HI);
	gdt_flush();
	tss_flush();

	current_task = task_new();
	current_task->page_directory = (dir_t *) virt_to_phys(PDIR_ADDR);
	current_task->state = TASK_READY;
	task_queue = current_task;
	sti();
}

void print_current_task()
{
	kprintf("[current_task] pid: %d, eip: %08x, esp:%08x, ebp: %08x, pd: %08x\n",
			current_task->pid, current_task->eip, current_task->esp, current_task->ebp, current_task->page_directory);
}

void ps()
{
	task_t *t = task_queue;
	while(t) {
		// no eip, esp - assumes we are already in kernel mode //
		kprintf("pid: %d, ppid: %d, state: %s, ring: %d\n", t->pid, t->ppid, task_states[t->state], t->ring);
		t = t->next;
	}
}
// gets the last task in task_queue
task_t *get_last_task()
{
	switch_locked = true;
	task_t *t = task_queue;
	while(t->next) {
		t = t->next;
	}
	switch_locked = false;
	return t;
}

// called from task_switch in sched.asm
task_t *task_switch_inner()
{
	if(switch_locked) {
		return current_task;
	}

	task_t *n = current_task->next;
	unsigned int i;
	for(i = 0; i < 10000; i++) {
		if(!n) {
			n = task_queue;
		}
		if(n->state == TASK_READY) {
			break;
		}
		n = n->next;
	}
	if(i == 10000) {
		panic("All threads sleeping?\n");
	}

	current_task = n;
	// set_tss_kernel_stack(n->tss_kernel_stack);
	// kprintf("switch to task %d\n", n->pid);
	// spin_unlock(&task_lock);
	return n;
}

// returns current running  task
task_t *get_current_task()
{
	return current_task;
}

task_t *fork_inner()
{

	switch_locked = true;
	task_t *t = task_new();
	t->ppid = current_task->pid;
	t->ring = current_task->ring;
	t->page_directory = clone_directory();

	// child has end addr as parent, clonned by clone directory //
	t->heap->end_addr = current_task->heap->end_addr;

	// clone files //
	t->files = (struct file **) calloc(current_task->num_files, sizeof(struct file *));
	int fd;
	for(fd = 0; fd < current_task->num_files; fd++) {
		if(!current_task->files[fd]) {
			continue;
		}
		t->files[fd] = calloc(1, sizeof(struct file));
		t->files[fd]->fs_node = current_task->files[fd]->fs_node;
		t->files[fd]->offs = current_task->files[fd]->offs;
		current_task->files[fd]->fs_node->ref_count++;
	}
	t->state = TASK_READY;
	switch_locked = false;
	return t;
}

pid_t getpid()
{
	return current_task ? current_task->pid : -1;
}

int getring()
{
	return current_task ? current_task->ring : -1;
}

task_t *get_task_by_pid(pid_t pid)
{
	task_t *t;
	for(t = task_queue; t; t = t->next) {
		if(t->pid == pid) {
			return t;
		}
	}
	return NULL;
}

void task_free(task_t *task)
{
	KASSERT(task);
	task_t *p; int fd;
	for(p = task_queue; p; p=p->next) {
		if(p->next == task) {
			p->next = task->next;
		}
	}
	KASSERT(task->wait_queue->num_items == 0);
	free_directory(task->page_directory);
	// closing wait queue //
	list_close(task->wait_queue);
	// closing it's files //
	for(fd = 0; fd < task->num_files; fd++) {
		if(task->files[fd]) {
			task->files[fd]->fs_node->ref_count--;
			if(task->files[fd]->fs_node->ref_count == 0){
				fs_close(task->files[fd]->fs_node);
			};
			free(task->files[fd]);
		}
	}
	free(task->files);
	free(task->heap);
	free(task);
}


pid_t task_wait(int *status)
{
	task_t *t;
	if(!current_task->wait_queue->num_items) {
		switch_locked = true;
		// check if they are not exiting childs; //
		bool found = false;
		for(t = task_queue; t; t = t->next) {
			if(t->ppid == current_task->pid) {
				found = true; break;
				kprintf("found task %d, in state: %s\n", t->pid, task_states[t->state]);
			}
		}
		if(! found) {
			switch_locked = false;
			kprintf("no childs\n");
			return -1;
		}
		current_task->state = TASK_SLEEPING;
		kprintf("Going to sleep\n");
		switch_locked = false;

		task_switch();

		kprintf("Wakeup\n");
	}

	switch_locked = true;
	// kprintf("Waiting for...%d items\n", current_task->wait_queue->num_items);
	node_t *n = current_task->wait_queue->head;
	t = (task_t *) n->data;
	pid_t pid = t->pid;
	if(status) *status = t->exit_status;
	task_free(t);
	list_del(current_task->wait_queue, n);
	switch_locked = false;
	return pid;
}

void task_exit(int status)
{
	// wait_queue_t *q, *n;
	task_t *t, *parent;
	// TODO - kill task and clean memory
	parent = get_task_by_pid(current_task->ppid);
	if(!parent) {
		// halt();
		return;
	}
	// reparent my children//
	for(t = task_queue; t; t = t->next) {
		if(t->ppid == current_task->pid) {
			t->ppid = 1;
		}
	}
	// clean my waiting q //
	node_t *n;
	switch_locked = true;
	for(n = current_task->wait_queue->head; n; n = n->next) {
		task_free((task_t *) n->data);
		list_del(current_task->wait_queue, n);
	}

	// add me to the parent waiting queue //
	list_add(parent->wait_queue, current_task);

	// and wake my parent //
	if(parent->state == TASK_SLEEPING) {
		parent->state = TASK_READY;
	}
	current_task->exit_status = status;
	current_task->state = TASK_EXITING;

	switch_locked = false;

	task_switch();
}
// void exit(int status) {
// 	task_exit(status);
// }


void switch_to_user_mode(uint32_t code_addr, uint32_t stack_hi_addr)
{
	current_task->ring = 3;
	switch_to_user_mode_asm(code_addr, stack_hi_addr);
}

//
//	Loading "init" from initrd.img "filesystem"
//		into memory @0x10000000 and jump to it in ring 3
//
void task_exec(char *path)
{
	fs_node_t *fs_node = fs_namei(path);
	if(!fs_node) {
		panic("Cannot find init %s\n", path);
	} else {
		kprintf("Loading /%s, inode:%d, at address %p, length:%d\n", fs_node->name,
				fs_node->inode, USER_CODE_START_ADDR, fs_node->length);
	}

	unsigned int num_pages = (fs_node->length / PAGE_SIZE) + 1;
	unsigned int i;
	for(i = 0; i < num_pages; i++) {
		map(USER_CODE_START_ADDR + (PAGE_SIZE*i), (unsigned int)frame_alloc(),
				P_PRESENT | P_READ_WRITE | P_USER);
	}
	// reserve 2 page stack //
	for(i = 2; i > 0; i--) {
		map(USER_STACK_HI-(i * PAGE_SIZE), (unsigned int) frame_calloc(),
			P_PRESENT | P_READ_WRITE | P_USER);
	}

	unsigned int offset = 0, size = 0;
	char *buff = (char *)USER_CODE_START_ADDR;
	do {
		size = fs_read(fs_node, offset, fs_node->length, buff);
		offset += size;
	} while(size > 0);

	switch_to_user_mode(USER_CODE_START_ADDR, USER_STACK_HI);
}
