//
// Created by vio on 27/01/16.
//
#ifndef _KBD_H
#define _KBD_H

/** 
 * Control codes are moved down, so A becomes 0x01 code
 */
// translate (move down 'A' becomes 0x01 code)
#define C(key) (key - '@')
// untranslate (move back up, 0x01 becomes 'A')
#define UC(key) (key + '@')
void kbd_install();
char kbdgetc();

#endif
