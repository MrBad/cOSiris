extern void console_init(void);
extern void scroll_up();
extern void scroll_down();
extern void clrscr(void);
extern void kprintf(char *fmt, ...);
extern void setxy(unsigned long r, unsigned long c);
extern void panic(char *fmt, ...);
