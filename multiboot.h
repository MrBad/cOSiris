#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <sys/types.h>

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
    uint32_t flags;			    // 0
    uint32_t mem_lower;		    // 4
    uint32_t mem_upper;		    // 8
    uint32_t boot_device;		// 12
    uint32_t cmdline;			// 16
    uint32_t mods_count;		// 20
    uint32_t mods_addr;		    // 24
    uint32_t syms[4];			// 28 - 44
    uint32_t mmap_length;		// 44
    uint32_t mmap_addr;		    // 48
    uint32_t drives_length;		// 52
    uint32_t drives_addr;		// 56
    uint32_t config_table;		// 60
    uint32_t boot_loader_name;	// 64
    uint32_t apm_table;		    // 68
    uint32_t vbe_control_info;	// 72
    uint32_t vbe_mode_info;	    // 76
    uint16_t vbe_mode;			// 80
    uint16_t vbe_interface_seg; // 82
    uint16_t vbe_interface_off; // 84
    uint16_t vbe_interface_len; // 86
    uint64_t fb_addr;
    uint32_t fb_pitch;
    uint32_t fb_width;
    uint32_t fb_height;
    uint8_t fb_bpp;
    uint8_t fb_type;
    uint32_t fb_color_info;
} multiboot_header;

typedef struct _memory_map {
    uint32_t size;
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} memory_map;

#endif
