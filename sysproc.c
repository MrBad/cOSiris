#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "console.h"
#include "vfs.h"
#include "mem.h"
#include "task.h"
#include "canonize.h"
#include "bname.h"
#include "rtc.h"
#include "syscall.h"
#include "sysproc.h"

int sys_kill(int pid, int signum)
{
    return kill(pid, signum);
}

sighandler_t sys_signal(int signum, sighandler_t handler)
{
    return signal(signum, handler);
}

/**
 * exec syscall
 * Usually you use this after a fork, an existing process
 * excepting if this process is init. If it's a fork, this exec will
 * replace initial process image with the new one.
 * this belongs to sys_proc, but...
 */
int sys_exec(char *path, char *argv[])
{
    fs_node_t *fs_node;
    char **tmp_argv = NULL;
    unsigned int argc = 0, i;

    // try to open path //
    if (fs_open_namei(path, O_RDONLY, 0, &fs_node) < 0) {
        char *p = canonize_path("/", path); // try to open from / root
        if (fs_open_namei(p, O_RDONLY, 0, &fs_node) < 0) {
            free(p);
            return -1;
        }
        free(p);
    }
    if (!(fs_node->type & FS_FILE)) {
        return -1;
    }
    // TODO try to check it's mask, if it's exec
    // not supported yet //

    // save argvs - we will destroy current process stack //
    // TODO - save this args on new stack
    uint32_t needed_mem = 0;
    if(argv) {
        tmp_argv = calloc(MAX_ARGUMENTS, sizeof(char *));
        for(argc = 0; argc < MAX_ARGUMENTS; argc++) {
            if(!argv[argc]) break;
            tmp_argv[argc] = strdup(argv[argc]);
            needed_mem += strlen(argv[argc]) + sizeof(char) + sizeof(uint32_t);
        }
    }
    set_default_signals(current_task->sig_handlers);
    // change name //
    if (current_task->name)
        free(current_task->name);
    current_task->name = strdup(fs_node->name);

    // map more pages if the new program needs more //
    // assign at least one more page of memory than file size,
    // to help pass exec params later
    unsigned int num_pages = (fs_node->size / PAGE_SIZE) + 2;
    for (i = 0; i < num_pages; i++) {
        virt_t page = USER_CODE_START_ADDR + PAGE_SIZE * i;
        if (!is_mapped(page)) {
            map(page, (unsigned int)frame_alloc(),
                    P_PRESENT | P_READ_WRITE | P_USER);
        }
    }

    // free it's stack //
    for (i = 4; i > 0; i--) {
        virt_t page = USER_STACK_HI - (i * PAGE_SIZE);
        if (!is_mapped(page)) {
            map(page, (unsigned int)frame_alloc(),
                    P_PRESENT | P_READ_WRITE | P_USER);
        }
        memset((void *)page, 0, PAGE_SIZE);
    }

    // Loads the program from disk
    // Currently we only use flat executables.
    // TODO: support elf
    unsigned int offset = 0, size = 0;
    char *buff = (char *)USER_CODE_START_ADDR;
    do {
        size = fs_read(fs_node, offset, fs_node->size, buff);
        offset += size;
    } while(size > 0);

    // After mapped num_pages, we have some free mem
    // Let's use it for passing arguments //
    // TODO: push the strings directly onto the stack, no need for this
    // hacky thing!
    void *free_mem_start = (void *) (buff + offset);
    unsigned int free_mem_size = num_pages * PAGE_SIZE - fs_node->size;
    // zero the rest of alloc space //
    memset(buff+offset, 0, free_mem_size);
    if (needed_mem > free_mem_size) {
        kprintf("Alloc another free page after code or clean this thing!\n");
        return -1;
    }
    /**
     * If we have 2 argv for example, we will have a memory like
     * code | free mem = new_argv | ptr0 | ptr1 | ptr1 points here 0 | ptr2 points here |
     */
    char *p;
    uint32_t *new_argv = (uint32_t *)free_mem_start;
    p = (char *)(new_argv + argc);
    for (i = 0; i < argc; i++) {
        strcpy(p, tmp_argv[i]);
        new_argv[i] = (uint32_t) p;
        int len = strlen(tmp_argv[i]);
        p[len]=0;
        p += len + 1;
        free(tmp_argv[i]);
    }
    if(tmp_argv)
        free(tmp_argv);

    uint32_t *stack = (uint32_t *)USER_STACK_HI;
    stack--;
    *stack = 0xC0DEBABE; // fake return point so we can catch it
    stack--;
    *stack = (uint32_t)new_argv; // push argv into stack //
    stack--;
    *stack = argc; // push argc into stack
    fs_close(fs_node);
    switch_to_user_mode(USER_CODE_START_ADDR, (uint32_t)stack);

    return 0;
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

