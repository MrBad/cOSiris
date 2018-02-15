#ifndef _ELF_H
#define _ELF_H

#define ELF_MAGIC "\177ELF"
#define EI_NIDENT 16

// Elf header
struct elf_header {
    uint8_t ident[16];  // ELF identification
    uint16_t type;      // Type, eg: ET_EXEC, ET_CORE
    uint16_t machine;   // Machine type - eg: EM_386
    uint32_t version;   // File version, != 0
    uint32_t entry;     // Process entry point virtual addr, or 0
    uint32_t phoffs;    // Offset in bytes to program header table, or 0
    uint32_t shoffs;    // Offset in bytes to section header table, or 0
    uint32_t flags;     // Processor specific flags    
    uint16_t ehsize;    // Elf header size, in bytes
    uint16_t phentsize; // Size in bytes of one entry in program header table
    uint16_t phnum;     // Number of entries in program header table
    uint16_t shentsize; // Size in bytes of one entry in section header
    uint16_t shnum;     // Number of entries in the program section header
    uint16_t shstrndx;  // index to entry of name string table
};

// Program header
struct prog_header {
    uint32_t type;  // Type of this segment, eg: PT_LOAD
    uint32_t offs;  // Offset (bytes) in file where this segment starts
    uint32_t vaddr; // Virtual address
    uint32_t paddr; // Physical address, not used
    uint32_t filesz;// Number of bytes in file image of the segment. Can be 0. 
    uint32_t memsz; // Number of bytes in memory image of the segment. - // -
    uint32_t flags; // Flags, 
    uint32_t align; // Alignment.
};

#define ET_EXEC 2   // Exec type file object
#define PT_LOAD 1   // type
#endif
