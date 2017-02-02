#ifndef _GDT_H
#define _GDT_H

#include <sys/types.h>

struct gdt_entry {
	unsigned short limit_low;
	unsigned short base_low;
	unsigned char base_middle;
	unsigned char access;
	unsigned char granularity;
	unsigned char base_high;
} __attribute__((packed));


struct gdt_ptr {
	unsigned short limit;
	unsigned int	base;
} __attribute__((packed));

// A struct describing a Task State Segment.
struct tss_entry_struct
{
	uint32_t prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
	uint32_t esp0;       // The stack pointer to load when we change to kernel mode.
	uint32_t ss0;        // The stack segment to load when we change to kernel mode.
	uint32_t esp1;       // Unused...
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;         // The value to load into ES when we change to kernel mode.
	uint32_t cs;         // The value to load into CS when we change to kernel mode.
	uint32_t ss;         // The value to load into SS when we change to kernel mode.
	uint32_t ds;         // The value to load into DS when we change to kernel mode.
	uint32_t fs;         // The value to load into FS when we change to kernel mode.
	uint32_t gs;         // The value to load into GS when we change to kernel mode.
	uint32_t ldt;        // Unused...
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

tss_entry_t tss;
void write_tss(uint32_t num, uint32_t ss0, uint32_t esp0);
void set_tss_kernel_stack(uint32_t *stack);

// declared in startup.asm - reload register segments //
extern void gdt_flush(void);
extern void tss_flush(void);
void gdt_init(void);


#endif
