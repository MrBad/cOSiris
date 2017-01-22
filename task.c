#include <types.h>
#include "x86.h"
#include "task.h"
#include "mem.h"
#include "kheap.h"
#include "isr.h"
#include "console.h"
#include "gdt.h"
#include "assert.h";


void print_int(unsigned int x)
{
	kprintf("print_int: %08x\n", x);
}

task_t *task_new()
{
	task_t *t = (task_t *) calloc(sizeof(task_t));
	t->pid = next_pid++;
	t->tss_esp_stack = malloc_page_aligned(PAGE_SIZE);
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
		kprintf("pid: %d, ppid: %d, eip: %08x, esp:%08x, ebp: %08x, pd: %08x\n", t->pid, t->ppid, t->eip, t->esp, t->ebp, t->page_directory);
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

extern void switch_to_user_mode_asm();
void switch_to_user_mode()
{
	KASSERT(current_task);
	set_kernel_stack((uint32_t) current_task->tss_esp_stack);
	switch_to_user_mode_asm();
}
