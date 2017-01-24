#include <types.h>
#include "x86.h"
#include "task.h"
#include "mem.h"
#include "kheap.h"
#include "isr.h"
#include "console.h"
#include "gdt.h"
#include "assert.h"
#include "vfs.h"


void print_int(unsigned int x)
{
	kprintf("print_int: %08x\n", x);
}

task_t *task_new()
{
	task_t *t = (task_t *) calloc(sizeof(task_t));
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
		kprintf("pid: %d, ppid: %d, ring: %d, eip: %08x, esp:%08x, ebp: %08x, pd: %08x\n", t->pid, t->ppid, t->ring, t->eip, t->esp, t->ebp, t->page_directory);
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

// gets the next task to be scheduled
task_t *get_next_task()
{
	current_task = current_task->next;
	if(!current_task) {
		current_task = task_queue;
	}
	return current_task;
}

// called from task_switch in sched.asm
task_t *task_switch_inner()
{
	task_t *n = get_next_task();
	set_tss_kernel_stack(n->tss_kernel_stack);
	return n;
}

// returns current running  task
task_t *get_current_task()
{
	return current_task;
}

int getpid()
{
	return current_task ? current_task->pid : -1;
}
int getring()
{
	return current_task ? current_task->ring : -1;
}

void task_exit(int status)
{
	// TODO - kill task and clean memory
	kprintf("exited %d, with status: %d\n", current_task->pid, status);
	sti();
	while(1) {
		nop(); // nop for now //
	}
}

void task_idle()
{
	while(1) {
		nop();
	}
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
		kprintf("Loading /%s at address %p, length:%d\n", fs_node->name,
				USER_CODE_START_ADDR, fs_node->length);
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
	kprintf("Loaded: %d bytes\n", offset);
	switch_to_user_mode(USER_CODE_START_ADDR, USER_STACK_HI);
}
