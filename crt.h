#ifndef _CRT_H
#define _CRT_H

#include <sys/types.h>
#include "ansi.h"

#define TAB_SPACES      8
#define CRTC_PORT       0x3D4
#define CRTC_DATA       0x3D5
#define CRT_ADDR        0xB8000
#define CRT_END_ADDR    0xC0000
#define CRT_COLS        80
#define CRT_ROWS        25
#define CRT_MAX_ROWS    CRT_ROWS
#define CRT_DEF_ATTR    0x700

// gets cursor position from crt structure
#define CRT_POS(crt) (crt.cols * crt.y + crt.x)
// sets n to be minimum one
#define MIN_ONE(crt, n) (n = crt.astat.n1 == 0 ? 1 : n)

// structure to keep track of the CRT
struct crt {
    uint16_t *buf;          // points to video memory
    uint16_t attr;          // attributes for color (bg, fg) moved up 8 bytes
    uint16_t cols, rows;    // number of colomns and rows
    uint16_t x, y;          // x and y position of the cursor (starts at 0,0)
    uint16_t scroll;        // where the scroll pointer is
    struct ansi_stat astat;
};

/**
 * Scrolls the display 10 lines by modifying video memory start address
 * This is hardware. In future we will use buffering in console.c
 * Also we will use an escape sequence for it.
 */
void crt_scroll_up();
void crt_scroll_down();

/**
 * Puts a character on the screen. 
 * This speak ANSI escape sequences(or a part of them)
 */
void crt_putc(char c);

/**
 * Inits video
 */
void crt_init();

#endif
