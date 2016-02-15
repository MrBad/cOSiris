OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
entry = 0x00100000;
SECTIONS {
	.text entry : AT (entry) {
		code = .;
		*(.text)
		. = ALIGN(4096);
	}
	textEnd = .;

	.data : AT(entry + (data - code)) {
		data = .;
		*(.data)
		*(.rodata)
		. = ALIGN(4096);
	}
	dataEnd = .;

	.bss : AT(entry + (bss - code)){
		bss = .;
		*(.common)
		*(.bss)
		. = ALIGN(4096);
	}
	. = ALIGN(4096);
	bssEnd = .;
	end = .;
}

