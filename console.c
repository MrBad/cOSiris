#define VGA_FB_ADDR 0xB8000
#define CRTC_IO_ADDR 0x3D4
#define SCR_COLS	80
#define SCR_ROWS	24
#define TAB_SPACES	8

#include "x86.h"
#include <stdarg.h>
#include <string.h>

extern void halt(void);

static char *vid_mem = (char *) VGA_FB_ADDR;
static unsigned long pos = 0;
static unsigned long row, col;
unsigned short attr = 0x07;


void clrscr(void){
	unsigned int i;	
	for(i=0; i < SCR_COLS*SCR_ROWS*2; i+=2) {
		vid_mem[i]= ' ';
		vid_mem[i+1] = attr;
	}
}
// update cursor position on screen //
void update_cursor (void){
	unsigned short offset;
	offset = (row * SCR_COLS + col);

	outb(CRTC_IO_ADDR+0, 0x0E);
	outb(CRTC_IO_ADDR+1, offset >> 8);
	outb(CRTC_IO_ADDR+0, 0x0F);
	outb(CRTC_IO_ADDR+1, offset);
}

// hardware scrolling by changing framebuffer base addr //
void scroll2row() {
	unsigned short lines;
	unsigned long offset;
	if(row > SCR_ROWS) {
		lines =  row - SCR_ROWS + 1;
	} else {
		lines = 0;
	}

	offset = lines * SCR_COLS;
	outb(CRTC_IO_ADDR+0, 0x0C);
	outb(CRTC_IO_ADDR+1, offset >> 8);
	outb(CRTC_IO_ADDR+0, 0x0D);
	outb(CRTC_IO_ADDR+1, offset & 0xFF);
}

void gotoxy(void){
	if(col >= SCR_COLS) {
		row++;
		col=0;
	}
	if(row >= SCR_ROWS) {
//		console_up();
	}
	pos = (row * SCR_COLS + col) * 2;
	scroll2row();
	update_cursor();
}

void clr2eol(void) {
	for(; col<SCR_COLS; col++) {
		gotoxy();
		vid_mem[pos] = ' ';
		vid_mem[pos+1] = attr;
	}
}
void clr2sol(void){
	for(;col > 0; col--) {
		gotoxy();
		vid_mem[pos] = ' ';
		vid_mem[pos+1] = attr;
	}
}
void put_tab(void) {
	unsigned int max;
	for(max=col+TAB_SPACES; col < max; col++) {
		gotoxy();
		vid_mem[pos] = ' ';
		vid_mem[pos+1] = attr;
	}
}

//
//	No tty like write yet
//
void console_write(char *str){
	for(; *str; str++) {
		
		switch (*str) {
			case '\r':
				clr2sol();
				break;
			
			case '\n':
				clr2eol();
				break;
				
			case '\t':
				put_tab();
				break;
				
			default:
				vid_mem[pos] = *str;
				vid_mem[pos+1] = attr;
				pos += 1;
				col++;
				break;
		}
		gotoxy();
	}
}


void kprintf(char *fmt, ...) {
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	
	console_write(buf);
	return;
}


void console_init(void) {
	clrscr();
	row=0; col=0; pos=0;
	gotoxy();
}


void setxy(unsigned long r, unsigned long c) {
	row = r;
	col = c;
	gotoxy();
}

void panic(char *fmt, ...){
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	console_write(buf);
	halt();
}
