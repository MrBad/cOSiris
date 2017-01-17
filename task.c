#include <types.h>
#include "x86.h"
#include "task.h"
#include "mem.h"
#include "kheap.h"
#include "isr.h"
#include "console.h"

extern unsigned int get_eip();
extern unsigned int get_esp();
extern unsigned int get_ebp();
extern dir_t *kernel_dir;


next_pid = 1;

void task1()
{
	unsigned int i;

	for(i = 0; i < 10000; i++) {
		if(i % 500) {
			kprintf("A");
		}
		nop();
	}
}

void task_init()
{
	asm volatile("cli");
	kprintf("TI\n");
	// init first task (kernel task)
	current_task = (task_t *) malloc(sizeof(task_t));
	current_task->pid = next_pid++;
	current_task->esp = current_task->ebp = 0; //
	current_task->eip = 0;
	current_task->page_directory = (dir_t *) virt_to_phys(PDIR_ADDR);
	current_task->next = NULL;
	ready_queue = current_task;
	kprintf("pd:%p\n", current_task->page_directory);
	asm volatile("sti");
}


int fork()
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

void task_switch()
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
int getpid() {
	return (current_task->pid);
}
