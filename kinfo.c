#include "console.h"
#include "kinfo.h"

/**
 * Defined in ASM. 
 * Will fill the kinfo struct - code, start, data, bss, end, size, stack, 
 *  stack_size
 */
extern uint32_t get_kinfo(struct kinfo *kinfo);

void get_kernel_info(uint32_t magic, multiboot_header *mb, struct kinfo *kinfo)
{
    memory_map *mm;
    struct minfo *minfo;

    uint8_t i = 0;
    get_kinfo(kinfo);
    
    if (magic != MBOOT_MAGIC) {
        panic("Invalid multiboot magic: %u\n", magic);
    }
    // Detect and save memory info from multiboot //
    if (mb->flags && (1 << MBOOTF_MMAP)) {
        mm = (memory_map *) mb->mmap_addr;
        do {
            if (!(mm->type & (MEM_USABLE|MEM_ACPI|MEM_ACPI_NVS)))
                continue;
            minfo = kinfo->minfo + i;
            minfo->base = (mm->base_addr_high << 16) + mm->base_addr_low;
            minfo->length = (mm->length_high << 16) + mm->length_low;
            minfo->type = mm->type;
            mm = (memory_map *) ((char *)mm + mm->size + sizeof(mm->size));
            i++;
            if (i > sizeof(kinfo->minfo) / sizeof(*kinfo->minfo)) {
                panic("get_kernel_info: Please increase minfo elements\n");
            }
        } while (mm < (memory_map *)(mb->mmap_addr + mb->mmap_length));
    } 
    else if (mb->flags && (1 << MBOOTF_MEM)) {
        kprintf("Multiboot loader did not pass memory map\n"
                "We will use mem_upper and assume starts at 1MB\n");
        minfo = &kinfo->minfo[0];
        minfo->base = 0x100000;
        minfo->length = mb->mem_upper * 1024;
        minfo->type = MEM_USABLE;
    }
    else {
        panic("Unable to detect memory\n");
    }
}
