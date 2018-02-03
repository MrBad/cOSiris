/**
 * Keyboard driver
 */
#include <sys/types.h>
#include <string.h>
#include "i386.h"
#include "irq.h"
#include "serial.h"
#include "console.h"
#include "kbd.h"
#include "ansi.h"
#include "tty.h"

/**
 * Codes of the non-ascii keys
 */
typedef enum {
    KLC  = 0x1D,    // left control
    KLS  = 0x2A,    // left shift
    KRS  = 0x36,    // right shift
    KLA  = 0x38,    // left alt
    KCL  = 0x3A,    // Caps lock
    KF1  = 0x3B,    // F1
    KF2  = 0x3C,    // F2
    KF3  = 0x3D,    // F3
    KF4  = 0x3E,    // F4
    KF5  = 0x3F,    // F5
    KF6  = 0x40,    // F6
    KF7  = 0x41,    // F7
    KF8  = 0x42,    // F8
    KF9  = 0x43,    // F9
    KF10 = 0x44,    // F10
    KNL  = 0x45,    // NumLock
    KSL  = 0x46,    // Scroll lock
    KHM  = 0x47,    // home
    KUP  = 0x48,    // cursor up
    KPGUP = 0x49,   // page up
    KLFT = 0x4B,    // cursor left
    KRGT = 0x4D,    // cursor right
    KEND = 0x4F,    // end
    KDWN = 0x50,    // cursor down
    KPGDWN = 0x51,  // page down
    KINS = 0x52,    // insert
    KDEL = 0x53,    // delete
    KF11 = 0x57,    // F11
    KF12 = 0x58,    // F12
} kbd_special_t;

uint8_t kbd_map[128] = {
    0,     0x1b,  '1',  '2',   '3',   '4',   '5',   '6', 
    '7',   '8',   '9',  '0',   '-',   '=',   '\b',  '\t', 
    'q',   'w',   'e',  'r',   't',   'y',   'u',   'i', 
    'o',   'p',   '[',  ']',   '\r',  KLC,   'a',   's',
    'd',   'f',   'g',  'h',   'j',   'k',   'l',   ';', 
    '\'',  '`',   KLS,  '\\',  'z',   'x',   'c',   'v', 
    'b',   'n',   'm',  ',',   '.',   '/',   KRS,   '*', 
    KLA,   ' ',   KCL,  KF1,   KF2,   KF3,   KF4,   KF5,
    KF6,   KF7,   KF8,  KF9,   KF10,  KNL,   KSL,   KHM,
    KUP,   KPGUP,   '-',  KLFT,   '5',   KRGT,   '+',   '1',
    KDWN,  KPGDWN,   '0',  '.',   0,     0,     0,     KF11, 
    KF12,  [0x5b] = 0x2
};

uint8_t kbd_map_shift[128] = {
    0,     0x1b,  '!',  '@',   '#',   '$',   '%',   '^', 
    '&',   '*',   '(',  ')',   '_',   '+',   '\b',  '\t', 
    'Q',   'W',   'E',  'R',   'T',   'Y',   'U',   'I', 
    'O',   'P',   '{',  '}',   '\r',  KLC,   'A',   'S',
    'D',   'F',   'G',  'H',   'J',   'K',   'L',   ':', 
    '"',   '~',   KLS,  '|',   'Z',   'X',   'C',   'V', 
    'B',   'N',   'M',  '<',   '>',   '?',   KRS,   '*', 
    KLA,   ' ',   KCL,  KF1,   KF2,   KF3,   KF4,   KF5,
    KF6,   KF7,   KF8,  KF9,   KF10,  KNL,   KSL,   '7',
    '8',   '9',   '-',  '4',   '5',   '6',   '+',   '1',
    '2',   '3',   '0',  '.',   0,     0,     0,     KF11, 
    KF12
};

/**
 * Bits of the keyboard status
 */
typedef enum {
    KBD_SHIFT   = 1 << 0,
    KBD_CTRL    = 1 << 1,
    KBD_CAPS    = 1 << 2,
    KBD_ALT     = 1 << 3,
    KBD_NUML    = 1 << 4,
    KBD_SCRLL   = 1 << 5,
    KBD_E0      = 1 << 6,
} kbd_stat_t;

/**
 * Status of special keys of the keyboard like CTRL, SHIFT, CAPSLK
 */
uint8_t kbd_stat;
#define KBD_BUF_OUT_SZ 30
struct kbd_out {
    uint8_t r, w;
    char buf[KBD_BUF_OUT_SZ];
} kbd_out;

void kbd_wbuf(char *str)
{
    char *p;
    for (p = str; *p && kbd_out.w - kbd_out.r < KBD_BUF_OUT_SZ; p++) {
        kbd_out.buf[kbd_out.w++ % KBD_BUF_OUT_SZ] = *p;
    }
}

int kbd_getc()
{
    if (kbd_out.r < kbd_out.w)
        return kbd_out.buf[kbd_out.r++ % KBD_BUF_OUT_SZ];

    if ((inb(0x64) & 0x01) == 0)
        return -1;
    uint8_t c = inb(0x60);
    bool is_p = (c & 0x80) == 0; // is a press event?
    //if (is_p)
    //    serial_debug("[orig:0x%x conv:0x%x]\n", c, c&~0x80);
    c = c & ~0x80;
    switch (c) {
        // Control keys //
        case KLC: 
            kbd_stat = is_p ? kbd_stat | KBD_CTRL : kbd_stat & ~KBD_CTRL; 
            return 0;
        case KLS:
        case KRS: 
            kbd_stat = is_p ? kbd_stat | KBD_SHIFT : kbd_stat & ~KBD_SHIFT; 
            return 0;
        case KLA: 
            kbd_stat = is_p ? kbd_stat | KBD_ALT : kbd_stat & ~KBD_ALT;
            return 0;
        case KCL: kbd_stat = is_p ? kbd_stat ^ KBD_CAPS : kbd_stat; return -1;
        case KNL: kbd_stat = is_p ? kbd_stat ^ KBD_NUML : kbd_stat; return -1;
        case KSL: kbd_stat = is_p ? kbd_stat ^ KBD_SCRLL : kbd_stat; return -1;
    }
    if (!is_p)
        return 0;

    switch (c) {
        // Send ANSI escape codes
        case KUP: kbd_wbuf("[A"); return ANSI_ESC;
        case KDWN: kbd_wbuf("[B"); return ANSI_ESC;
        case KRGT: kbd_wbuf("[C"); return ANSI_ESC;
        case KLFT: kbd_wbuf("[D"); return ANSI_ESC;
        case KPGUP: kbd_wbuf("[5~"); return ANSI_ESC;
        case KPGDWN: kbd_wbuf("[6~"); return ANSI_ESC;
    }

    if (kbd_stat & KBD_SHIFT) {
        c = kbd_map_shift[c];
    } else if (kbd_stat & KBD_CTRL) {
        // control codes will be translated down, so CTRL-A becomes 0x01
        c = kbd_map_shift[c];
        if (c < 'A' || c > 'Z')
            return 0;
        c = C(c);
    } else {
        c = kbd_map[c];
        if (kbd_stat & KBD_CAPS)
            if (c >= 'a' && c <= 'z')
                c = c - 'a' + 'A';
    }

    return c;
}

void kbd_handler(struct iregs *r)
{
    (void) r;
    //console_in(kbd_getc);
    char c;
    while (((c = kbd_getc()) > 0)) {
        tty_in(tty_devs[0], c);
    }
}

void kbd_install()
{
    irq_install_handler(0x1, kbd_handler);
}
