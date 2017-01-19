#include <types.h>
#include "x86.h"
#include "task.h"
#include "mem.h"
#include "kheap.h"
#include "isr.h"
#include "console.h"


int next_pid = 1;

void print_int(unsigned int x) {
	kprintf("print_int: %08x\n", x);
}

task_t *task_new() {
	task_t *t = (task_t *) calloc(sizeof(task_t));
	t->pid = next_pid++;
	return t;
}

void task_init() {
	cli();
	current_task = task_new();
	current_task->page_directory = (dir_t *) virt_to_phys(PDIR_ADDR);
	task_queue = current_task;
	sti();
}

void print_current_task(){
	kprintf("[current_task] pid: %d, eip: %08x, esp:%08x, ebp: %08x, pd: %08x\n",
			current_task->pid, current_task->eip, current_task->esp, current_task->ebp, current_task->page_directory);
}
void ps() {
	task_t *t = task_queue;
	while(t) {
		kprintf("pid: %d, eip: %08x, esp:%08x, ebp: %08x, pd: %08x\n", t->pid, t->eip, t->esp, t->ebp, t->page_directory);
		t = t->next;
	}
}

task_t *get_last_task() {
	task_t *t = task_queue;
	while(t->next) {
		t = t->next;
	}
	return t;
}

task_t *get_next_task() {
	current_task = current_task->next;
	if(!current_task) {
		current_task = task_queue;
	}
	return current_task;
}
task_t *get_current_task(){
	return current_task;
}

task_t *fork_inner() {
	task_t *t = task_new();
	t->page_directory = clone_directory();
	return t;
}

#if 0
int fork_old()
{
	task_t *parent_task, *new_task;
	cli();
	parent_task = current_task;

	new_task = (task_t *) malloc(sizeof(task_t));


	new_task->pid = next_pid++;
	new_task->esp = new_task->ebp = 0;
	new_task->eip = 0;
	new_task->next = 0;
	task_t *t = (task_t *)ready_queue;
	while(t->next) {
		t = t->next;
	}
	new_task->page_directory = clone_directory();
	t->next = new_task;
	unsigned int eip = get_eip(); // entry point for new process //
	// here we can be the parent or the newly child when we sti is on //
	if(current_task == parent_task) {
		// if we are in parent task //
		unsigned int esp;
		asm volatile("mov %%esp, %0" : "=r"(esp));
       	unsigned int ebp;
		asm volatile("mov %%ebp, %0" : "=r"(ebp));
		new_task->eip = eip;
		new_task->esp = esp;
		new_task->ebp = ebp;
		// nop();
		// kprintf("Parent: esp:%p, ebp:%p, eip:%p\n", current_task->esp, current_task->ebp, current_task->eip);
		// debug_dump_list(first_block);
		asm volatile("sti");; // let the timer switch
		return new_task->pid;
	} else {
		// if(current_task->esp == 0) {
			// task_switch();
		// }
		// debug_dump_list(first_block);
		// kprintf("Child: esp:ct: %p, %p, ebp:%p, eip:%p\n", current_task, current_task->esp, current_task->ebp, current_task->eip);
		// kprintf("Switched to child!\n");
		return 0; // child
	}
}

void task_switch_old()
{
	unsigned int esp, ebp, eip;
	if(!current_task) {
		return;
	}
	// kprintf(".");
	asm volatile("mov %%esp, %0" : "=r"(esp));
	asm volatile("mov %%ebp, %0" : "=r"(ebp));
	eip = get_eip();
	if(eip == 0x12345) {
		return;
	}
	current_task->eip = eip;
	current_task->esp = esp;
	current_task->ebp = ebp;
	current_task = current_task->next;
	if(!current_task) {
		current_task = ready_queue;
	}
	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;
	// cli();
	// kprintf("current eip:%p, esp:%p, ebp:%p\n", eip, esp, ebp);
	// sti();
	asm volatile("cli");
	asm volatile("movl %0, %%ecx" : :"r"(eip));
	asm volatile("movl %0, %%esp" : :"r"(esp));
	asm volatile("movl %0, %%ebp" : :"r"(ebp));
	asm volatile("movl %0, %%cr3" : :"r"(current_task->page_directory));
	// switch_page_directory(current_task->page_directory);
	asm volatile("movl $0x12345, %eax");
	asm volatile("sti");
	asm volatile("jmp *%ecx");
}
#endif
int getpid() {
	if(current_task) {
		return (current_task->pid);
	} else {
		return -1;
	}
}
