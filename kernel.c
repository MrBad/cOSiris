#include "kernel.h"
#include <sys/types.h>
#include <string.h>
#include "x86.h"
#include "console.h"
#include "multiboot.h"
#include "kinfo.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "timer.h"
#include "delay.h"
#include "kbd.h"
#include "rtc.h"
#include "mem.h"
#include "kheap.h"
#include "vfs.h"
#include "initrd.h"
#include "serial.h"
#include "task.h"
#include "syscall.h"
#include "zero.h"
#include "pipe.h"
#include "hd_queue.h"
#include "cofs.h"

uint32_t initrd_location, initrd_end;

void main(uint32_t magic, multiboot_header *mboot)
{
    (void) magic;
    
    // multiboot_parse(magic, mboot);
    kprintf("\ncOSiris...\n");

    get_kernel_info(magic, mboot, &kinfo);
    kprintf("Kernel info:\n");
    kprintf(" code start: 0x%p, start entry: 0x%p, data start: 0x%p\n", 
            kinfo.code, kinfo.start, kinfo.data);
    kprintf(" bss start: 0x%p, end: 0x%p, total size: %dKB\n", 
            kinfo.bss, kinfo.end, kinfo.size/1024);
    kprintf(" stack (low/end): 0x%p, size: 0x%p\n", 
            kinfo.stack, kinfo.stack_size);

    kprintf("Setup gdt entries\n");
    gdt_init();

    kprintf("Setup idt entries\n");
    idt_init();

    kprintf("Setup isr\n");
    isr_init();

    kprintf("Setup irq\n");
    irq_install();

    serial_init();
    sti();

    kprintf("Setup timer\n");
    timer_install();

    rtc_init();

    kprintf("Calibrating delay loop: %d loops/ms\n", calibrate_delay_loop());

    kprintf("Setup keyboard\n");
    kbd_install();

    // find location of initial ramdisk //
    if (mboot->mods_count > 0) {
        initrd_location = *((uint32_t *) mboot->mods_addr);
        initrd_end = *(uint32_t *) (mboot->mods_addr + 4);
    }

    kprintf("Setup paging\n");
    mem_init(mboot);
    // fs_root = initrd_init(initrd_location);

    fs_root = cofs_init();
    console_init();
    task_init();
    syscall_init();
    // wait for the rtc interrupt to fire //
    while (!system_time)
        ;
    kprintf("Unix time: %u\n", system_time);

    sys_exec("/init", NULL);

    // ps();
    kprintf("Should not get here\n");
    return;
}
