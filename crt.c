#include <stdarg.h>
#include <string.h>
#include "i386.h"
#include "crt.h"

uint8_t ansi_to_col[8] = 
{
    0, 4, 2, 6, 1, 5, 3, 7
};

struct crt crt;

int crt_get_cursor()
{
    int pos;
    outb(CRTC_PORT, 0x0E);
    pos = inb(CRTC_DATA) << 8;
    outb(CRTC_PORT, 0x0F);
    pos |= inb(CRTC_DATA);
    return pos;
}

void crt_set_cursor(int pos)
{
    if (pos < 0)
        pos = CRT_MAX_ROWS * crt.cols - pos;
    pos = pos % (CRT_MAX_ROWS * crt.cols);
    crt.x = pos % crt.cols;
    crt.y = pos / crt.cols;
    
    outb(CRTC_PORT, 0x0E);
    outb(CRTC_PORT + 1, pos >> 8);
    outb(CRTC_PORT, 0x0F);
    outb(CRTC_PORT + 1, pos & 0xFF);
}

void crt_cursor_on()
{
    outb(CRTC_PORT, 0x0A);
    outb(CRTC_DATA, (inb(CRTC_DATA) & 0xC0) | 0x00);
    outb(CRTC_PORT, 0x0B);
    outb(CRTC_DATA, (inb(0x3E0) & 0xE0) | 0x0E);
}

void crt_cursor_off()
{
    outb(CRTC_PORT, 0x0A);
    outb(CRTC_DATA, 0x20);
}

void crt_set_cursor_xy(uint16_t x, uint16_t y)
{
    if (y > CRT_MAX_ROWS)
        return;
    if (x > crt.cols)
        return;
    int pos = y * crt.cols + x;
    crt_set_cursor(pos);
}

static void crt_scroll_to_line(int line)
{
    int offs;
    line = line < crt.rows ? crt.rows : line;
    line = line > crt.y ? crt.y : line;
    offs = line > crt.rows ? (line - crt.rows) * crt.cols : 0;
    crt.scroll = line;
    outb(CRTC_PORT, 0x0C);
    outb(CRTC_DATA, offs >> 8);
    outb(CRTC_PORT, 0x0D);
    outb(CRTC_DATA, offs & 0xFF);
}

void crt_reset()
{
    int i;
    crt.x = crt.y = 0;
    crt.attr = CRT_DEF_ATTR;
    for (i = 0; i < crt.cols * CRT_MAX_ROWS; i++) {
        crt.buf[i] = ' ' | crt.attr;
    }
    crt_set_cursor(0);
    crt_scroll_to_line(0);
}

void crt_init()
{
    int pos = crt_get_cursor();
    crt.buf = (uint16_t *) CRT_ADDR;
    crt.attr = CRT_DEF_ATTR;
    crt.cols = CRT_COLS;
    crt.rows = CRT_ROWS;
    crt.x = pos % crt.cols;
    crt.y = pos / crt.cols;
    crt_cursor_on();
}

static void crt_set_color(uint8_t color, uint8_t bright)
{
    int bg_color = crt.attr >> 8 & (0xF << 4);
    if (bright)
        color = color + 8;
    color = color & 0x0F; 
    crt.attr = (bg_color | color ) << 8;
}

static void crt_set_background(uint8_t bg_color, uint8_t bright)
{
    int color = (crt.attr >> 8) & 0x0F;
    if (bright)
        bg_color = bg_color + 8;
    bg_color = (bg_color & 0x0F) << 4;
    crt.attr = (bg_color | color) << 8;
}

void crt_handle_ansi(char c)
{
    int n1 = crt.astat.n1, n2 = crt.astat.n2;
    if (c == 'c') {
        crt_reset();
    } else if (c == 'm') {
        if (n1 == 0)
            crt.attr = CRT_DEF_ATTR;
        else if (n1 >= 30 && n1 <= 37)
            crt_set_color(ansi_to_col[n1 - 30], n2);
        else if (n1 >= 40 && n1 <= 47)
            crt_set_background(ansi_to_col[n1 - 40], n2);
    } else if (c == 'A') {
        MIN_ONE(crt, n1);
        crt_set_cursor(CRT_POS(crt) - n1 * crt.cols);
    } else if (c == 'B') {
        MIN_ONE(crt, n1);
        crt_set_cursor(CRT_POS(crt) + n1 * crt.cols);
    } else if (c == 'C') {
        MIN_ONE(crt, n1);
        crt_set_cursor(CRT_POS(crt) + n1);
    } else if (c == 'D') {
        MIN_ONE(crt, n1);
        crt_set_cursor(CRT_POS(crt) - n1);
    } else {
        //serial_debug("\nGot command: %c\n", c);
    }
}

void crt_scroll_up()
{
    crt_scroll_to_line(crt.scroll - 10);
}

void crt_scroll_down()
{
    crt_scroll_to_line(crt.scroll + 10);
}

void crt_putc(char c)
{
    if (ansi_stat_switch(&crt.astat, c)) {
        if (crt.astat.stat == ANSI_IN_FIN) {
            crt_handle_ansi(c);
        }
        return;
    }
    if (c == '\n' || crt.x == crt.cols) {
        crt.y++;
        crt.x = 0;
    } else if (c == '\r') {
        crt.x = 0;
    } else if (c == '\f') {
        crt.y++;
    } else if (c == '\b') {
        if (crt.x == 0 && crt.y > 0) {
            crt.x = crt.cols;
            crt.y--;
        }
        crt.x--;
    } else if (c == '\t') {
        crt.x = (crt.x + TAB_SPACES) & ~(TAB_SPACES - 1);
    } else {
        crt.buf[CRT_POS(crt)] = c | crt.attr;
        crt.x++;
    }
    if (crt.x > 80) {
        crt.x = 0;
        crt.y++;
    }
    if (crt.y == CRT_MAX_ROWS) {
        crt.y--;
        memmove(crt.buf, crt.buf+crt.cols, (crt.cols*2) * (crt.y));
        for (int i = 0; i < crt.cols; i++)
            crt.buf[crt.y * crt.cols + i] = ' ' | crt.attr;
    }
    if (crt.x == 0)
        crt_scroll_to_line(crt.y);
    crt_set_cursor_xy(crt.x, crt.y);
}
