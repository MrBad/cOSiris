#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "assert.h"
#include "console.h"
#include "vfs.h"
#include "mem.h"
#include "task.h"
#include "canonize.h"
#include "bname.h"
#include "rtc.h"
#include "syscall.h"
#include "sysproc.h"
#include "elf.h"

int sys_kill(int pid, int signum)
{
    return kill(pid, signum);
}

sighandler_t sys_signal(int signum, sighandler_t handler)
{
    return signal(signum, handler);
}

/**
 * Exec syscall
 * Usually you use this after forking an existing process, except "init".
 * Exec will replace user process image if exists with new one, 
 * loaded from file path, which should be in ELF format (check bin/link.ld).
 * It can load the ELF on any address, as long as it fits between 
 *  USER_CODE_START_ADDR - UHEAP_START
 * Already open files will be preserved, signals will be set to default.
 * Process will see a new stack, a new heap and a new image.
 * The old process ring 3 memory will be free.
 * Arguments will be pushed to the new stack, as in main(int argc, char *argv[])
 */
int sys_exec(char *path, char *argv[])
{
    char *bin_path;
    fs_node_t *node;
    struct elf_header elf;
    struct prog_header prg;
    int i, offs;
    uint32_t pg, npgs, n, *u_esp, new_argv;
    virt_t vpage;
    int argc = 0, len;
    char *tmp_argv[MAX_ARGUMENTS];

    if (fs_open_namei(path, O_RDONLY, 0, &node) < 0) {
        bin_path = canonize_path("/", path); // we should use PATH here
        if (fs_open_namei(bin_path, O_RDONLY, 0, &node) < 0) {
            free(bin_path);
            //errno = ENOENT;
            return -1;
        }
        free(bin_path);
    }
    if (S_ISREG(node->type) == 0)
        return -1;
    //if ((node->mask & (S_IXUSR | S_IXOTH | S_IXUSR)) == 0)
    //    return -1;
    if (fs_read(node, 0, sizeof(elf), (char *)&elf) != sizeof(elf))
        return -1;
    if (memcmp(elf.ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0)
        return -1;
    if (elf.type != ET_EXEC)
        return -1;

    /* Save args, because user prog image, heap and stack will be wiped out. */
    if (argv) {
        for (argc = 0; *argv && argc < MAX_ARGUMENTS; argc++, argv++)
            tmp_argv[argc] = strdup(*argv);
    }
    tmp_argv[argc] = NULL;

    /* This is the point of no return. I'm not coming back, dear! */

    // Set default signals
    set_default_signals(current_task->sig_handlers);

    // Free old proc image
    for (pg = 0; USER_CODE_START_ADDR + pg * PAGE_SIZE < UHEAP_START; pg++) {
        vpage = USER_CODE_START_ADDR + pg * PAGE_SIZE;
        if (!is_mapped(vpage))
            break;
        free_page(vpage);
    }

    // Free old proc heap
    heap_t *h = current_task->heap;
    for (vpage = h->start_addr; vpage < h->end_addr; vpage += PAGE_SIZE)
        free_page(vpage);
    h->end_addr = h->start_addr;

    offs = elf.phoffs;

    for (i = 0; i < elf.phnum; i++) {
        if (fs_read(node, offs, sizeof(prg), (char *)&prg) != sizeof(prg))
            exit(128 + SIGKILL);
        if (prg.type & PT_LOAD && prg.filesz > 0) {
            if (prg.vaddr < USER_CODE_START_ADDR) {
                kprintf("vaddr: %#x, expected > %#x, abort\n",
                    prg.vaddr, USER_CODE_START_ADDR);
                exit(128 + SIGKILL);
            }
            if (prg.vaddr + prg.memsz > UHEAP_START) {
                kprintf("vaddr: %#x, expected < %#x, abort\n",
                    prg.vaddr, UHEAP_START);
                exit(128 + SIGKILL);
            }
            if (prg.align != PAGE_SIZE) {
                kprintf("bad alignment: %#x, expected %#x",
                    prg.align, PAGE_SIZE);
                exit(128 + SIGKILL);
            }
            // Map pages as needed
            npgs = (prg.memsz / prg.align) + 1;
            for (pg = 0; pg < npgs; pg++) {
                vpage = prg.vaddr + pg * prg.align;
                if (!is_mapped(vpage))
                    map(vpage, (uint32_t) frame_alloc(),
                        P_PRESENT | P_USER | P_READ_WRITE); // should be RO
            }
            // Initialize .bss to 0
            for (pg = prg.filesz / prg.align; pg < npgs; pg++)
                memset((void *)(prg.vaddr + pg * prg.align), 0, prg.align);
            // Load the program image
            n = fs_read(node, prg.offs, prg.filesz, (char *)prg.vaddr);
            if (n != prg.filesz) {
                kprintf("Can't load program\n");
                exit(128 + SIGKILL);
            }
        }
        offs += elf.phentsize;
    }

    // Set new name
    if (current_task->name)
        free(current_task->name);
    current_task->name = strdup(node->name);

    // Stack. Keep min 4 clean pages, free the rest if exists.
    vpage = USER_STACK_HI - PAGE_SIZE;
    for (i = 0; vpage > USER_STACK_LOW; i++, vpage -= PAGE_SIZE) {
        if (i < USER_STACK_MIN_PGS) {
            if (!is_mapped(vpage))
                map(vpage, (uint32_t) frame_alloc(),
                    P_PRESENT | P_USER | P_READ_WRITE);
            memset((void *)vpage, 0, PAGE_SIZE);
        } else if (is_mapped(vpage)) {
            free_page(vpage);
        } else {
            break;
        }
    }
    // Push saved argv strings into stack
    u_esp = (uint32_t *) USER_STACK_HI;
    for (i = 0; i < argc; i++) {
        // esp should always be 4 bytes aligned.
        len = 4 + ((strlen(tmp_argv[i]) + 1) & ~0x3);
        u_esp = (uint32_t *)((uint32_t)u_esp - len);
        memcpy(u_esp, tmp_argv[i], len);
        free(tmp_argv[i]);
        tmp_argv[i] = (char *)u_esp;
    }

    // Push argv pointers. argv[argc] has a NULL, save it too.
    for (i = argc; i >= 0; i--) {
        *--u_esp = (uint32_t)tmp_argv[i];
    }
    new_argv = (uint32_t)u_esp; // Save it, this will be out new argv.
    *--u_esp = 0xC0DEBABE;      // Fake return address.
    *--u_esp = new_argv;        // int main(argc, argv) ;)
    *--u_esp = argc;

    fs_close(node);

    // Let the switch jump to ring3, no return
    switch_to_user_mode(elf.entry, (uint32_t)u_esp);

    return (128 + SIGKILL);
}

pid_t sys_setsid()
{
    if (current_task->uid)
        return -1;
    if (current_task->leads)
        return -1;
    current_task->leads = true;
    current_task->sid = current_task->pgrp = current_task->pid;
    current_task->tty = -1;

    return current_task->pgrp;
}

time_t sys_time(time_t *tlock)
{
    time_t ret;

    if (tlock)
        validate_usr_ptr(tlock);
    ret = system_time;
    if (tlock)
        *tlock = ret;

    return ret;
}

