#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "console.h"
#include "kbd.h"
#include "irq.h"
#include "serial.h"
#include "vfs.h"
#include "x86.h"
#include "task.h"
#include "pipe.h"
#include "initrd.h"

#define CRTC_PORT	0x3D4
#define SCR_COLS	80
#define SCR_ROWS	24
#define TAB_SPACES	8
#define BACKSPACE	0x100

static uint16_t * vid_mem = (uint16_t *)VID_ADDR;
static uint16_t attr = 0x700; 
static spin_lock_t console_lock;
static int scroll_pos;

//
//	pos = col + 80 * row
//
static int get_cursor_pos()
{
    int pos;
    outb(CRTC_PORT, 14);
    pos = inb(CRTC_PORT+1) << 8;
    outb(CRTC_PORT, 15);
    pos |= inb(CRTC_PORT+1);
    return pos;
}

static void set_cursor_pos(int pos)
{
    outb(CRTC_PORT, 14);
    outb(CRTC_PORT+1, pos >> 8);
    outb(CRTC_PORT, 15);
    outb(CRTC_PORT+1, pos);
    // vid_mem[pos] = ' ' | attr;
}


static void scroll2pos(int pos)
{
    int rows = pos / SCR_COLS;
    int offs;
    if(rows > SCR_ROWS) {
        offs = (rows - SCR_ROWS) * SCR_COLS;
    } else {
        offs = 0;
    }
    // serial_debug("offs: %d, pos:%d, r: %d\n", offs, pos, pos/SCR_COLS);
    outb(CRTC_PORT, 0x0c);
    outb(CRTC_PORT+1, offs >> 8);
    outb(CRTC_PORT, 0x0d);
    outb(CRTC_PORT+1, offs & 0xFF);
}

void scroll_line_up()
{
    scroll_pos -= SCR_COLS;
    if(scroll_pos < SCR_COLS * SCR_ROWS) scroll_pos = SCR_COLS*SCR_ROWS;
    scroll2pos(scroll_pos);
}

void scroll_line_down()
{
    int curr_pos = get_cursor_pos();
    scroll_pos += SCR_COLS;
    if(scroll_pos > curr_pos) scroll_pos = curr_pos;
    scroll2pos(scroll_pos);
}

void scroll_page_up()
{
    scroll_pos -= SCR_COLS*SCR_ROWS;
    if(scroll_pos < SCR_COLS * SCR_ROWS) scroll_pos = SCR_COLS*SCR_ROWS;
    scroll2pos(scroll_pos);
}

void scroll_page_down()
{
    int curr_pos = get_cursor_pos();
    scroll_pos += SCR_COLS*SCR_ROWS;
    if(scroll_pos > curr_pos) scroll_pos = curr_pos;
    scroll2pos(scroll_pos);
}

static void console_putc(char c)
{
    int pos = get_cursor_pos(),
        i;
    if(c == '\n') {
        pos += SCR_COLS - pos % SCR_COLS;
        scroll2pos(pos);
    } else if(c == '\b') {
        vid_mem[--pos] = ' ' | attr;
    } else if(c == '\t') {
        i = pos % TAB_SPACES == 0 ? TAB_SPACES : TAB_SPACES - (pos % TAB_SPACES);
        while(i-- > 0) vid_mem[pos++] = ' ' | attr;
    } else {
        vid_mem[pos++] = c | attr;
    }
    // if we reached end of vid memory - 0xC0000;
    pos = pos % 16384; // TODO - fix memory overflow,maybe use it like a circular buffer
    if(pos % SCR_COLS == 0) {
        scroll2pos(pos);
    }
    scroll_pos = pos;
    set_cursor_pos(pos);
}

void clrscr()
{
    int i;
    for(i = 0; i < 16384; i++) {
        vid_mem[i] = ' '|attr;
    }
    set_cursor_pos(0);
}

void panic(char * str, ...)
{
    char buf[1024];
    int i;
    va_list args;
    va_start(args, str);
    vsprintf(buf, str, args);
    va_end(args);
    // spin_lock(&console_lock);
    serial_debug("%s", str);
    for(i = 0; i < 1024; i++) {
        if(! buf[i]) break;
        console_putc(buf[i]);
    }
    cli();
    halt();
}

void kprintf(char *fmt, ...)
{
    char buf[1024];
    unsigned int i;
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    serial_write(buf);
    buf[1023] = 0;
    spin_lock(&console_lock);
    for(i = 0; i < 1024; i++) {
        if(buf[i]==0) break;
        console_putc(buf[i]);
    }
    spin_unlock(&console_lock);
}

unsigned int console_write(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
    (void) node;
    (void) offset;
    unsigned int i;
    // spin_lock(&console_lock);
    for(i = 0; i < size; i++) {
        console_putc(buffer[i]);
    }
    serial_debug("%s", buffer);
    // spin_unlock(&console_lock);
    return i;
}

// keyboard //
#define KBD_BUFF_SIZE 10
struct {
    char buff[KBD_BUFF_SIZE];
    uint16_t r;
    uint16_t w;
    uint16_t e;
} kbd_buff;


extern bool ctrl_pressed;

list_t * cons_wait_queue;

void console_handler(struct iregs *r)
{
    unsigned char c;
    static int chr_count = 0; // number of characters in a input row
    // spin_lock(&console_lock);
    if (r->eax == 2)
        c = r->ebx;
    else 
        c = kbdgetc();
    if(!c) 
        return;
    if(c == '\b') { // if backspace - for now is hardcoded to erase prev char
        if(chr_count > 0) {
            kbd_buff.e--;
            chr_count--;
            console_putc(c);
            serial_putc(c); serial_putc(' '); serial_putc(c);
        }
        return;
    }
    if(kbd_buff.e - kbd_buff.r < KBD_BUFF_SIZE) {
        c = (c=='\r') ? '\n' : c;
        kbd_buff.buff[kbd_buff.e++ % KBD_BUFF_SIZE] = c;
        console_putc(c);
        serial_putc(c);
        chr_count++;
        if(c == '\n' || kbd_buff.e == kbd_buff.r + KBD_BUFF_SIZE) {
            kbd_buff.w = kbd_buff.e;
            chr_count=0;
            wakeup(&kbd_buff);
            // node_t *n; // waking up procs waiting on read //
            // for(n = cons_wait_queue->head; n; n = n->next) {
            // 	if(!n) break;
            // 	// kprintf("waking up: %d\n", ((task_t *)n->data)->pid);
            // 	((task_t *)n->data)->state = TASK_READY;
            // 	list_del(cons_wait_queue, n);
            // }
        }
    }
    // outb(0x20, 0x20);
    // spin_unlock(&console_lock);
}

unsigned int console_read(fs_node_t *node, unsigned int offset, unsigned int size, char *buffer)
{
    (void) node;
    (void) offset;
    unsigned char c;
    unsigned initial_size = size;
    // spin_lock(&console_lock);
    while(size > 0) {
        // serial_debug("Should read %d bytes\n", size);
        while(kbd_buff.r == kbd_buff.w) {
            if(current_task->state == TASK_EXITING) {
                //spin_unlock(&console_lock);
                return -1;
            }
            if(current_task->state == TASK_READY) {
                // serial_debug("Nothing to read, going to sleep: %d\n", current_task->pid);
                sleep_on(&kbd_buff);
                //list_add(cons_wait_queue, current_task);
                //current_task->state = TASK_SLEEPING;
                //task_switch();
            }
        }

        c = kbd_buff.buff[kbd_buff.r++ % KBD_BUFF_SIZE];
        *buffer++ = c;
        size--;
        if(c == '\n') break;
    }
    // spin_unlock(&console_lock);
    return initial_size-size;
}

#if 0
static fs_node_t *create_console_device()
{
    fs_node_t * n = calloc(1, sizeof(fs_node_t));
    strcpy(n->name, "console");
    n->inode = 0;
    n->uid = n->gid = 0;
    n->mask = 0666;
    n->flags = FS_CHARDEVICE;
    n->read = console_read;
    n->write = console_write;
    n->open = 0;
    n->close = 0;
    n->readdir = 0;
    n->finddir = 0;
    n->size = 1;
    return n;
}
#endif

void console_close(struct fs_node *node)
{
    // always keep console node in memory //
    //if(node->ref_count > 1)
    node->ref_count--;
}

void console_init()
{
    cons_wait_queue = list_open(NULL);
    irq_install_handler(0x1, console_handler);
    // fs_mount("/dev/console", create_console_device());
    fs_node_t *cons = fs_namei("/dev/console");
    if(!cons) {
        panic("Cannot find /dev/console\n");
    }
    if(!(cons->type & FS_CHARDEVICE)) {
        panic("/dev/console is not a char device\n");
    }
    cons->read = console_read;
    cons->write = console_write;
    cons->close = console_close;
    cons->open = NULL;
    // cons->ref_count--;
}
