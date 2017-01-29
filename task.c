#include "include/types.h"
#include "x86.h"
#include "task.h"
#include "mem.h"
#include "kheap.h"
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
	kprintf("%d", x);
}

task_t *task_new()
{
	task_t *t = (task_t *) calloc(1, sizeof(task_t));
	t->pid = next_pid++;
	t->tss_kernel_stack = (unsigned int *)KERNEL_STACK_HI;//malloc_page_aligned(PAGE_SIZE);
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
	set_tss_kernel_stack(current_task->tss_kernel_stack);
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
	task_t *t = task_queue;
	while(t->next) {
		t = t->next;
	}
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
	for(i=0; i < 10000; i++) {
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
	switch_locked = true;

	KASSERT(task);
	task_t *p;
	for(p = task_queue; p; p=p->next) {
		if(p->next == task) {
			p->next = task->next;
		}
	}
	KASSERT(task->wait_queue == NULL);
	// kprintf("free %d\n", task->pid);
	free_directory(task->page_directory);
	free(task);
	switch_locked = false;
}

pid_t task_wait(int *status)
{

	// do we have any child who can wake me up? //
	task_t *t; bool found = false;
	for(t = task_queue; t; t = t->next) {
		if(t->ppid == current_task->pid && t->state < TASK_EXITING) {
			found = true; break;
		}
	}
	if(!found) {
		return -1; // no child left to wake me up //
	}
	// kprintf("task %d put to sleep\n", current_task->pid);
	current_task->state = TASK_SLEEPING;
	task_switch();

	wait_queue_t *q = current_task->wait_queue;
	if(q) {
		current_task->wait_queue = q->next;
		if(status) *status = q->status;
		pid_t pid = q->pid;
		free(q);
		task_free(get_task_by_pid(pid));
		return pid;
	}
	kprintf("task_wait(): Cannot find child who waked me up!\n");
	return -1;
}

void task_exit(int status)
{
	wait_queue_t *q, *n;
	task_t *t, *parent;
	// TODO - kill task and clean memory
	parent = get_task_by_pid(current_task->ppid);
	if(!parent) {
		return;
	}
	// reparent children//
	for(t = task_queue; t; t = t->next) {
		if(t->ppid == current_task->pid) {
			t->ppid = 1;
		}
	}
	// clean waiting q //
	for(q = current_task->wait_queue; q; q = q->next) {
		current_task->wait_queue = q->next;
		kprintf("Cleaning: %d", q->pid);
		task_free(get_task_by_pid(q->pid));
		free(q);
	}
	// add me to the parent waiting queue //
	n = calloc(1, sizeof(wait_queue_t));
	n->pid = current_task->pid;
	n->status = status;
	q = parent->wait_queue;
	if(!q) {
		parent->wait_queue = n;
	} else {
		while(q->next) q = q->next;
		q->next = n;
	}
	// and wakeup parent //
	if(parent->state == TASK_SLEEPING) {
		parent->state = TASK_READY;
	}
	current_task->exit_status = status;
	current_task->state = TASK_EXITING;
}


void switch_to_user_mode(uint32_t code_addr, uint32_t stack_hi_addr)
{
	current_task->ring = 3;
	switch_to_user_mode_asm(code_addr, stack_hi_addr);
}

//
//	Loading "init" from initrd.img "filesystem"
//		into memory @0x10000000 and jump to it in ring 3
//
void exec_init()
{
	extern fs_node_t *fs_root;
	if(!fs_root) {
		panic("File system not inited");
	}
	fs_node_t *fs_node = finddir_fs(fs_root, "init");
	if(!fs_node) {
		panic("Cannot find init\n");
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
		size = read_fs(fs_node, offset, fs_node->length, buff);
		offset += size;
	} while(size > 0);
	// kprintf("Loaded: %d bytes\n", offset);
	switch_to_user_mode(USER_CODE_START_ADDR, USER_STACK_HI);
}
