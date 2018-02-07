#include <stdarg.h>
#include <string.h>
#include "assert.h"
#include "i386.h"
#include "serial.h"
#include "crt.h"

uint8_t ansi_to_col[8] = 
{
    0, 4, 2, 6, 1, 5, 3, 7
};

struct crt crt;

static int crt_get_cursor()
{
    int pos;
    outb(CRTC_PORT, 0x0E);
    pos = inb(CRTC_DATA) << 8;
    outb(CRTC_PORT, 0x0F);
    pos |= inb(CRTC_DATA);
    return pos;
}

static void crt_set_cursor(int pos)
{
    outb(CRTC_PORT, 0x0E);
    outb(CRTC_DATA, (pos >> 8) & 0x0FF);
    outb(CRTC_PORT, 0x0F);
    outb(CRTC_DATA, pos & 0xFF);
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
    KASSERT(y < CRT_MAX_ROWS);
    KASSERT(x < crt.cols);
    crt.x = x;
    crt.y = y;
    crt_set_cursor(CRT_POS(crt));
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

static void crt_reset()
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
    int pos;
    crt.buf = (uint16_t *) CRT_ADDR;
    crt.attr = CRT_DEF_ATTR;
    crt.cols = CRT_COLS;
    crt.rows = CRT_ROWS;
    pos = crt_get_cursor();
    if (pos / crt.cols > crt.rows - 1)
        pos = 0;
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

static void crt_reverse_video()
{
    int bg_color = crt.attr & 0x0F;
    int text_color = (crt.attr) >> 8 & 0x0F;
    crt_set_color(bg_color, 0);
    crt_set_background(text_color, 0);
}

static void ansi_unknown(struct ansi_stat *st)
{
    serial_debug("Unknown ansi. c1: %c, c2: %c, n1: %d, n2: %d\n",
        st->c1, st->c2, st->n1, st->n2);
}

static void crt_handle_ansi(char c)
{
    int n1 = crt.astat.n1,
        n2 = crt.astat.n2,
        i;

    if (c == 'c') {
        crt_reset();
    } else if (c == 'm') {
        if (n1 == 0)
            crt.attr = CRT_DEF_ATTR;
        else if (n1 >= 30 && n1 <= 37)
            crt_set_color(ansi_to_col[n1 - 30], n2);
        else if (n1 >= 40 && n1 <= 47)
            crt_set_background(ansi_to_col[n1 - 40], n2);
        else if (n1 == 7)
            crt_reverse_video();
        else if (n1 == 39)
            crt_set_background(CRT_DEF_ATTR & 0x0F, 0);
        else
            ansi_unknown(&crt.astat);
    } else if (c == 'A') {
        MIN_ONE(crt, n1);
        crt_set_cursor_xy(crt.x, crt.y - n1);
    } else if (c == 'B') {
        MIN_ONE(crt, n1);
        crt_set_cursor_xy(crt.x, crt.y + n1);
    } else if (c == 'C') {
        MIN_ONE(crt, n1);
        crt_set_cursor_xy(crt.x + n1, crt.y);
    } else if (c == 'D') {
        MIN_ONE(crt, n1);
        crt_set_cursor_xy(crt.x - n1, crt.y);
    } else if (c == 'K') { // erase in line
        if (n1 == 1) {
            for (i = 0; i < crt.x; i++)
                crt.buf[crt.y * crt.cols + i] = ' ' | crt.attr;
        } else if (n1 == 2) {
            for (i = 0; i < crt.cols; i++)
                crt.buf[crt.y * crt.cols + i] = ' ' | crt.attr;
        } else {
            for (i = crt.x; i < crt.cols; i++)
                crt.buf[crt.y * crt.cols + i] = ' ' | crt.attr;
        }
    } else if (crt.astat.n1 == 25 && crt.astat.c2 == '?') {
        if (c == 'h')
            crt_cursor_on();
        else if (c == 'l')
            crt_cursor_off();
    } else if (c == 'H') { // move cursor
        MIN_ONE(crt, n1);
        MIN_ONE(crt, n2);
        crt_set_cursor_xy(n2 - 1, n1 - 1);
    } else {
        ansi_unknown(&crt.astat);
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

/**
 * Line feed
 */
static void crt_lf()
{
    int i;
    crt.y--;
    memcpy(crt.buf, crt.buf + crt.cols, sizeof(crt.buf[0]) * crt.cols * crt.y);
    for (i = 0; i < crt.cols; i++)
        crt.buf[crt.y * crt.cols + i] = ' ' | crt.attr;
}

/**
 * Char out crt routine - needs rewrite
 * TODO: This code is not beautiful and it needs change.
 */
void crt_putc(char c)
{
    if (ansi_stat_switch(&crt.astat, c)) {
        if (crt.astat.stat == ANSI_IN_FIN) {
            crt_handle_ansi(c);
        }
        return;
    }
    if (crt.x > crt.cols) {
        crt.x %= crt.cols;
        crt.y++;
        if (CRT_POS(crt) >= crt.rows * crt.cols)
            crt_lf();
    }
    if (c == '\n') {
        crt.y++;
        if (CRT_POS(crt) >= crt.rows * crt.cols)
            crt_lf();
    } else if (c == '\r') {
        crt.x = 0;
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
    crt_set_cursor(CRT_POS(crt));
}

