#ifndef _CONSOLE_H
#define _CONSOLE_H

#define VGA_FB_ADDR 0xB8000

void console_init(void);
void scroll_up();
void scroll_down();
void clrscr(void);
void kprintf(char *fmt, ...);
void setxy(unsigned long r, unsigned long c);
void panic(char *fmt, ...);
void console_write(char *str);

#endif
