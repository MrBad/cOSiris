OUTPUT_FORMAT("elf32-i386")    /* output flat binary */

SECTIONS
{
	. = 0x10000000;        /* relocate to address 0x10000000 */

	.text ALIGN(4):
	{
		crt0.o(.text)     /* include the .text section of start.o */
		*(.text)           /* include all other .text sections */
	}

	.data :
	{
	    . = ALIGN(4);
		*(.data)
		*(.rodata*)
	}
}
