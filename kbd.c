#include "include/types.h"
#include "serial.h"
#include "console.h"
#include "x86.h"
#include "irq.h"

char kbd_buff[256];
unsigned int kbd_buff_ptr = 0;

unsigned char kbd_map[128] = {
		0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
		'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
		0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
		0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
		'*',
		'0',
		' ',
		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0,
		0,
		'-',
		0,
		0,
		0,
		'+',
		0,
		0,
		0,
		0,
		0,
		0, 0, 0,
		0,
		0,
		0
};

unsigned char kbd_map_shift[128] = {
		0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
		'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
		0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
		0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
		'*',
		'0',
		' ',
		0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0,
		0,
		'-',
		0,
		0,
		0,
		'+',
		0,
		0,
		0,
		0,
		0,
		0, 0, 0,
		0,
		0,
		0
};

bool shift_pressed = false;

void kbd_handler() {
	char scancode = 0;
	if(inb(0x64) & 1)
	scancode = inb(0x60);
//	kprintf("%X ", scancode);
//	return;
	if(scancode & 0x80) {
		// when key release //
		if(scancode == 0x2A || scancode == 0x36) {
			shift_pressed = false;
			return;
		}
	} else {
		scancode = scancode & 0x7F;
		// 0x0E -> backspace
		// 0x49 -> page up
		// 0x48 -> a up
		// 0x4B -> a left
		// 0x4D -> a right
		// 0x50 -> a down
		// 0x51 -> page down
		// 0x47 -> home
		// 0x4F -> end
		serial_debug("%X ", scancode);
		if(scancode == 0x48) {
			scroll_up();
			return;
		}
		if(scancode == 0x50) {
			scroll_down();
			return;
		}
		if (scancode == 0x1) {
			kprintf("Shut down\n");
			char *c = "Shutdown";
			while (*c) {
				outb(0x8900, *c++);
			}
		}
//		kprintf("%c", scancode);
		// when key pressed //
		if(scancode == 0x2A || scancode == 0x36) {
			shift_pressed = true;
			return;
		}
		if(shift_pressed) {
			kprintf("%c", kbd_map_shift[scancode]);
		} else {
			kprintf("%c", kbd_map[scancode]);
		}
	}
	return;
}

void kbd_install() {
	irq_install_handler(0x1, kbd_handler); // seteaza timerul pe intreruperea 0
	return;
}
