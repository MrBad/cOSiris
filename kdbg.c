/**
 * kdbg - Kernel GDB remote stub.
 * Note: Breakpoints in threads (processes) are globals right now.
 *       Use the light version of .gdbinit.
 * Parts of the code are inspired by gdb/stubs/
 * Docs: https://sourceware.org/gdb/onlinedocs/gdb/Remote-Protocol.html
 *       https://www.embecosm.com/appnotes/ean4/embecosm-howto-rsp-server-ean4-issue-2.html
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2018 MrBadNews <viorel dot irimia at gmail dot come>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "i386.h"
#include "console.h"
#include "dev.h"
#include "tty.h"
#include "serial.h"
#include "int.h"
#include "kdbg.h"
#include "task.h"
#include "mem.h"

#define BUFSIZE 512
#define KDBG_STACK_SIZE 1024 /* kdbg stack size, in number of elements */
#define BREAK_OPC 0xCC       /* op code for i386 int3 */
#define MAX_BREAK_POINTS 64

/**
 * An internal representation of a breakpoint.
 * Right now the pid does not make too much sense, a breakpoint being fired
 * when any process hits it, but the idea is to have per process breakpoints
 * in the future. Setting them and reseting them is made by gdb, on each break.
 */
struct break_point {
    char *addr;         /* Address in memory of the breakpoint */
    unsigned char val;  /* Previous value before we INT3OP it */
    pid_t pid;          /* Process who ads it */
};

/**
 * We will use a different stack for kdbg handler, because we are switching
 * contexts and things can become fishy.
 */
uint32_t kdbg_stack[1024];
/* Stack grows downwords */
uint32_t kdbg_stack_hi = (uint32_t)kdbg_stack + KDBG_STACK_SIZE;
static char kdbg_in[BUFSIZE];   /* Input buffer */
static char kdbg_out[BUFSIZE];  /* Output buffer*/
static int kdbg_reply = 0;      /* should we send a reply on the next break? */
static int kdbg_pid = 0;        /* pid of the process that made a break */
static char hexchars[] = "0123456789abcdef";
static struct break_point bpts[MAX_BREAK_POINTS]; /* Breakpoints array */

enum regnames { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
        EIP,
        EFLAGS,
        CS, SS, DS, ES, FS, GS, NUMREGS };

/**
 * Get a character from serial line, in pooling mode.
 * We are using pooling mode, because this should work even 
 *  when interrupts are disabled.
 */
static char getDebugChar()
{
    char c;
    while ((c = serial_getc(KDBG_TTY)) < 0)
        nop();
        ;
    return c;
}

/**
 * Put a character to serial line
 */
static void putDebugChar(char c)
{
    serial_putc(c, KDBG_TTY);
}

/**
 * Try to translate from i386 interrupt vector number to Unix signal value
 */
static int computeSignal(int int_no)
{
    int sig_vals[] = { 8,  5,  7,  5, 16, 16,  4, 8, 
                       7, 11, 11, 11, 11, 11, 11, 7, 7 };
    if (int_no < 16)
        return sig_vals[int_no];
    return 7;
}

/**
 * Converts a hex number representation to binary.
 */
static int hex(char c)
{
    if (c >= '0' && c <= '9' )
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/**
 * Read a packet and send it's ack
 */
static char *kdbg_get_packet()
{
    char c;
    unsigned char cksum, xcksum;
    int i;
    char *buf = &kdbg_in[0];

    for (;;) {
        while ((c = getDebugChar()) != '$')
            ;
    retry:
        
        for (i = 0, cksum = 0, xcksum = 0; i < BUFSIZE - 1; i++) {
            c = getDebugChar();
            if (c == '$')
                goto retry;
            if (c == '#')
                break;
            buf[i] = c;
            cksum += c;
        }
        buf[i] = 0;
        if (c != '#') {
            kprintf("kdbg: buffer too small\n");
            continue;
        }
        c = getDebugChar();
        xcksum = hex(c) << 4;
        c = getDebugChar();
        xcksum += hex(c);

        if (cksum != xcksum) {
            kprintf("kdbg: bad checksum. cnt: %#x, sent: %x, buf: %s\n",
                    cksum, xcksum, buf);
            putDebugChar('-'); // failed, retry
        } else {
            //kprintf("-> [%s]\n", buf);
            putDebugChar('+'); // success
            if (buf[2] == ':') {
                putDebugChar(buf[0]);
                putDebugChar(buf[1]);
                return &buf[3];
            }
            return &buf[0];
        }
    }
}

/**
 * Send a packet, computing it's checksum, waiting for it's acknoledge.
 * If there is not ack, packet will be resent.
 */
static void kdbg_put_packet(char *buf)
{
    uint8_t cksum;
    //kprintf("<- [%s]\n", buf);
    for (;;) {
        putDebugChar('$');
        for (cksum = 0; *buf; buf++) {
            cksum += *buf;
            putDebugChar(*buf);
        }
        putDebugChar('#');
        putDebugChar(hexchars[cksum >> 4]);
        putDebugChar(hexchars[cksum % 16]);
        if (getDebugChar() == '+')
            break;
    }
}

/**
 * Returns a ptr to last char put in buf or NULL on error (cannot read memory)
 */
static char *mem2hex(char *mem, char *buf, int size)
{
    int i;
    unsigned char c;
    char hexchars[] = "0123456789abcdef";

    for (i = 0; i < size; i++, mem++) {
        if (!is_mapped((virt_t)mem & ~0xFFF))
            return NULL;
        c = *mem;
        *buf++ = hexchars[c >> 4];
        *buf++ = hexchars[c % 16];
    }
    *buf = 0;
    return buf;
}

/**
 * Returns a ptr to the char after last memory byte written
 *  or NULL on error (cannot write memory)
 */
static char *hex2mem(char *buf, char *mem, int size)
{
    int i;
    unsigned char c;

    for (i = 0; i < size; i++, mem++) {
        if (!is_mapped((virt_t)mem & ~0xFFF)) {
            kprintf("not mapped %x\n", mem);
            return NULL;
        }
        c = hex(*buf++) << 4;
        c += hex(*buf++);
        *mem = c;
    }
    return mem;
}

/**
 * Translates from iregs structure to a registers buffer that gdb expects.
 */
static void regs2buf(struct iregs *r, uint32_t *regs)
{
    int i = 0;
    regs[i++] = r->eax;
    regs[i++] = r->ecx;
    regs[i++] = r->edx;
    regs[i++] = r->ebx;
    regs[i++] = r->esp;
    regs[i++] = r->ebp;
    regs[i++] = r->esi;
    regs[i++] = r->edi;
    regs[i++] = r->eip;
    regs[i++] = r->eflags;
    regs[i++] = r->cs;
    regs[i++] = r->ss;
    regs[i++] = r->ds;
    regs[i++] = r->es;
    regs[i++] = r->fs;
    regs[i++] = r->gs;
}

/**
 * Translates from gdb registers buffer to iregs structure
 */
static void buf2regs(uint32_t *regs, struct iregs *r)
{
    int i = 0;
    r->eax = regs[i++];
    r->ecx = regs[i++];
    r->edx = regs[i++];
    r->ebx = regs[i++];
    r->esp = regs[i++];
    r->ebp = regs[i++];
    r->esi = regs[i++];
    r->edi = regs[i++];
    r->eip = regs[i++];
    r->eflags = regs[i++];
    r->cs = regs[i++];
    r->ss = regs[i++];
    r->ds = regs[i++];
    r->es = regs[i++];
    r->fs = regs[i++];
    r->gs = regs[i++];
}

/**
 * Sets a breakpoint at address (TODO: for current process)
 */
static int set_break_point(uint32_t addr)
{
    uint32_t i, sz;
    sz = sizeof(bpts) / sizeof(*bpts);

    for (i = 0; i < sz; i++) {
        if (bpts[i].addr == 0)
            break;
    }
    if (i == sz) {
        kprintf("breakpoints full\n");
        return -1;
    }
    if (!is_mapped((virt_t)addr & ~0xFFF)) {
        kprintf("cannot set break_point: %x\n", addr);
        return -1;
    }
    bpts[i].addr = (char *) addr;
    bpts[i].val = *bpts[i].addr;
    bpts[i].pid = current_task->pid;
    *bpts[i].addr = BREAK_OPC;
    return 0;
}

/**
 * Reset breakpoint at addr (TODO: for current process)
 */
static int reset_break_point(uint32_t addr, pid_t pid)
{
    (void) pid;
    uint32_t i, sz;
    sz = sizeof(bpts) / sizeof(*bpts);

    for (i = 0; i < sz; i++) {
        if ((uint32_t)bpts[i].addr == addr) {
            break;
        }
    }
    if (i == sz) {
        return -1;
    }
    *bpts[i].addr = bpts[i].val;
    memset(bpts + i, 0, sizeof(*bpts));
    return 0;
}


/**
 * Called when an exception occurs, by kdbg_handler_enter. Does not return.
 * All registers are already saved in current_task->regs, by int_handler.
 */
void kdbg_handler()
{
    int signal, stepping = 0;
    pid_t pid, last_pid;
    task_t *task;
    uint32_t addr, size;
    uint32_t regs[NUMREGS];
    char *ptr, op;

    kdbg_pid = getpid();
    signal = computeSignal(current_task->regs.int_no);

    /* If we halted after a cCsS previous packet, inform gdb we stopped. */
    if (kdbg_reply) {
        snprintf(kdbg_out, sizeof(kdbg_out), "S%02x", signal);
        kdbg_put_packet(kdbg_out);
        kdbg_reply = 0;
    }

    for (;;) {
        ptr = kdbg_get_packet();
        kdbg_out[0] = 0;

        /* Tell gdb why we stopped */
        if (*ptr == '?') {
            snprintf(kdbg_out, sizeof(kdbg_out), "S%02x", signal);
        }
        /* Send the registers values - g */
        else if (*ptr == 'g') {
            regs2buf(&current_task->regs, regs);
            mem2hex((char *) regs, kdbg_out, NUMREGS*sizeof(uint32_t));
        }
        /* Set the registers values - G XX..  */
        else if (*ptr == 'G') {
            hex2mem(++ptr, (char *) regs, NUMREGS*sizeof(uint32_t));
            buf2regs(regs, &current_task->regs);
            strcpy(kdbg_out, "OK");
        }
        /* Read memory mAAAAAAAA,LL */
        else if (*ptr == 'm') {
            if (sscanf(kdbg_in, "m%8x,%x", &addr, &size) != 2) {
                kprintf("kdbg: error parsing %s\n", kdbg_in);
                strcpy(kdbg_out, "E01");
            } else if (size * 2 > sizeof(kdbg_in) - 1) {
                kprintf("kdbg: in %s - buffer too big\n", kdbg_in);
                strcpy(kdbg_out, "E01");
            } else {
                if (mem2hex((char *)addr, kdbg_out, size) == NULL) {
                    strcpy(kdbg_out, "E03");
                    //kprintf("Mem fault: %#x\n", addr);
                }
            }
        }
        /* Write memory  MAAAAAAAA,LLLL:XX.. */
        else if (*ptr == 'M') {
            if (sscanf(kdbg_in, "M%x,%x", &addr, &size) != 2) {
                kprintf("kdbg: error parsing %s\n", kdbg_in);
                strcpy(kdbg_out, "E02");
            } else {
                if ((ptr = strchr(kdbg_in, ':'))) {
                    ptr++;
                    if (hex2mem(ptr, (char *) addr, size) == NULL) {
                        strcpy(kdbg_out, "E03");
                        //kprintf("Mem fault %#x\n", addr);
                    } else {
                        strcpy(kdbg_out, "OK");
                        kprintf("WM:[%s]\n", kdbg_in);
                    }
                }
            }
        }
        /* step, continue */
        else if (*ptr == 's' || *ptr == 'c') {
            stepping = *ptr == 's' ? 1 : 0;
            ptr++;
            if (sscanf(ptr, "%x", &addr))
                current_task->regs.eip = addr;
            if (stepping)
                current_task->regs.eflags |= 0x100; /* Set trap flag. */
            else
                current_task->regs.eflags &= ~0x100; /* Clear trap flag */
            kdbg_reply = 1;
            break;
        }
        /* Queries, WIP */
        else if (*ptr == 'q') {
            /* Send current process id */
            if (*(ptr + 1) == 'C') {
                snprintf(kdbg_out, sizeof(kdbg_out), "QC%x", getpid());
            }
            /* Send first pid in thread list */
            else if (strncmp(ptr, "qfThreadInfo", 12) == 0) {
                task = task_queue;
                last_pid = task->pid;
                snprintf(kdbg_out, sizeof(kdbg_out), "m%x", task->pid);
            }
            /* Send next thread pid, or l (el) if no more*/
            else if (strncmp(ptr, "qsThreadInfo", 12) == 0) {
                if (!(task = get_task_by_pid(last_pid))) {
                    strcpy(kdbg_out, "E01");
                } else {
                    task = task->next;
                    if (task) {
                        last_pid = task->pid;
                        snprintf(kdbg_out, sizeof(kdbg_out), "m%x", task->pid);
                    } else {
                        strcpy(kdbg_out, "l");
                    }
                }
            }
            /* Send more infos about a thread */
            else if (strncmp(ptr, "qThreadExtraInfo", 15) == 0) {
                if (sscanf(ptr, "qThreadExtraInfo,%x", &pid) == 1) {
                    task = get_task_by_pid(pid);
                    char str[128], states[] = "CRASE";
                    int n;
                    n = snprintf(str, sizeof(str), "%s, %c",
                            task->name, states[task->state]);
                    mem2hex(str, kdbg_out, n);
                } else {
                    strcpy(kdbg_out, "E01");
                }
            }
            /* Send some supported features */
            else if (strncmp(ptr, "qSupported", 9) == 0) {
                snprintf(kdbg_out, sizeof(kdbg_out), 
                    "qSupported:PacketSize=%x", sizeof(kdbg_in));
            }
            else if(strcmp(ptr, "qAttached") == 0) {
                strcpy(kdbg_out, "OK");
            }
        }
        /* Is this thread still alive? */
        else if(*ptr == 'T') {
            if (sscanf(kdbg_in, "T%x", &pid) == 1) {
                if (get_task_by_pid(pid)) {
                    strcpy(kdbg_out, "OK");
                } else {
                    strcpy(kdbg_out, "E01");
                }
            }
        }
        /* Switch task context and prepare for the next instruction from gdb */
        else if (*ptr == 'H') {
            op = *++ptr;
            ptr++;
            if (*ptr == '-' && *(ptr+1) == '1')
                pid = -1;
            else if (!sscanf(ptr, "%x", &pid)) {
                kprintf("kdbg: in H op, bad pid %s\n", ptr);
                kdbg_put_packet("E01");
                continue;
            }
            pid = pid <= 0 ? kdbg_pid : pid;
            if (!strchr("mMgGcs", op)) {
                kprintf("kdbg: unknown H op %c\n", op);
                kdbg_put_packet("E01");
                continue;
            }
            if (!(task = get_task_by_pid(pid))) {
                kprintf("kdbg: H, pid %x died?\n", pid);
                kdbg_put_packet("E01");
                continue;
            }
            if ((op == 'c' || op == 's') && task->state != TASK_RUNNING) {
                kprintf("kdbg: %d is not running\n", task->pid);
                kdbg_put_packet("E01");
                continue;
            }
            current_task = task;
            switch_pd(task->page_directory);
            kdbg_put_packet("OK");
            continue;
        }
        else if (*ptr == 'k') {
            kprintf("kdbg: kill\n");
            break;
        }
        else if (*ptr == 'Z' || *ptr == 'z') {
            op = *ptr++;

            if (sscanf(ptr, "0,%x,1", &addr) == 1) {
                if (op == 'Z') {
                    if (set_break_point(addr) < 0)
                        strcpy(kdbg_out, "E01");
                    else
                        strcpy(kdbg_out, "OK");
                } else if (op == 'z') {
                    if (reset_break_point(addr, current_task->pid) < 0) 
                        strcpy(kdbg_out, "E01");
                    else 
                        strcpy(kdbg_out, "OK");
                }
            }
        }
        else {
            kprintf("kdbg: not supported ->[%s]\n", kdbg_in);
        }

        kdbg_put_packet(kdbg_out);
    }
    /* jump to current task and load registers (continue) */
    int_return(&current_task->regs);
}

/**
 * Init kernel gdb remote stub.
 */
int kdbg_init()
{
    int i;
    memset(bpts, 0, sizeof(bpts));
    for (i = 0; i < 4; i++)
        int_install_handler(i, kdbg_handler_enter);

    return 0;
}

