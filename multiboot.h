#define testb(buf, bit) (buf & (1 << bit))


#define MBOOT_MAGIC 0x2BADB002
#define MBOOTF_MEM		0	// multiboot memory flag
#define MBOOTF_BOOTD	1	// multiboot boot device flag
#define MBOOTF_ELF		5	//
#define MBOOTF_MMAP		6	// memory map struct

#define MEM_USABLE      1
#define MEM_RESERVED    2
#define MEM_ACPI        3
#define MEM_ACPI_NVS    4
#define MEM_BAD         5

typedef struct _multiboot_header {
	unsigned long flags;			// 0
	unsigned long mem_lower;		// 4
	unsigned long mem_upper;		// 8
	unsigned long boot_device;		// 12
	unsigned long cmdline;			// 16
	unsigned long mods_count;		// 20
	unsigned long mods_addr;		// 24
	unsigned long syms[4];			// 28 - 44
	unsigned long mmap_length;		// 44
	unsigned long mmap_addr;		// 48
	unsigned long drives_lenth;		// 52
	unsigned long drives_addr;		// 56
	unsigned long config_table;		// 60
	unsigned long boot_loader_name;	// 64
	unsigned long apm_table;		// 68
	unsigned long vbe_control_info;	// 72
	unsigned long vbe_mode_info;	// 76
	unsigned long vbe_mode;			// 80
	unsigned short vbe_interface_seg;// 82
	unsigned short vbe_interface_off;// 84
	unsigned short vbe_interface_len;// 86
} multiboot_header;

typedef struct _memory_map {
	unsigned long size;				
	unsigned long base_addr_low;	
	unsigned long base_addr_high;
	unsigned long length_low;
	unsigned long length_high;
	unsigned long type;
} memory_map;


void multiboot_parse(unsigned int magic, multiboot_header *mboot);