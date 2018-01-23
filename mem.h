#ifndef _MEM_H
#define _MEM_H

#include <sys/types.h>
#include "multiboot.h"

// A memory page has 4k
#define PAGE_SIZE 4096

// Flags for pages
#define P_PRESENT           1<<0 // Is this page present?
#define P_READ_WRITE        1<<1 // If set, write is enabled, else is read only
#define P_USER              1<<2 // If set, page is accessible in user mode
#define P_WRITE_THROUGH     1<<3 // if set, write-through caching enabled 
                                 //    else write-back enabled
#define P_CACHE_DISABLED    1<<4 // Ff set, cache will be disabled for page
#define P_ACCESSED          1<<5 // Was page read or written to?
#define P_DIRTY             1<<6 // For PT. If set, page was accessed
#define P_SIZE              1<<7 // For PD - if set, pages ar 4M, else 4K
#define P_GLOBAL            1<<8 // If set, prevents TLB from updating
                                 //     the cache if CR3 is reset
/**
 * Helper types
 */
typedef uint32_t phys_t;    // A physical memory address type.
typedef uint32_t virt_t;    // A virtual memory address type.
typedef uint32_t dir_t;     // Directory type.
typedef uint16_t flags_t;   // Flags type.

/**
 * Memory map 
 */
// Where the user program will be loaded in memory
#define USER_CODE_START_ADDR 0x10000000
// Where the user program stack is located. 
// Stack grows downwards
// High address; We leave an empty unmapped page to track errors in stack
//   access. That's it, if a user program will access this page, a fault will
//   be generated.
#define USER_STACK_HI   (USER_CODE_START_ADDR - PAGE_SIZE)
// User program low stack address (or max stack)
#define USER_STACK_LOW	(USER_STACK_HI - 10 * PAGE_SIZE)
// Kernel stack high address
#define KERNEL_STACK_HI		0xFF118000ul
// A reserved kernel page, to map in any physical memory page, so we can 
//   alter it when paging is enabled
#define RESV_PAGE			0xFFBFE000ul
// This is using recursive mapping...
// Where in virtual memory page for tables is located
#define PTABLES_ADDR 		0xFFC00000ul
// Where in virtual memory page for directory is located
#define PDIR_ADDR			0xFFFFF000ul

/**
 * Initialize the memory
 */
void mem_init();

/**
 * Kernel heap sbrk
 */
void *sbrk(int increment);

/**
 * User heap sbrk
 */
void *sys_sbrk(int increment);

/**
 * Maps a physical page to a virtual one, with flags
 */
void map(virt_t virtual_addr, phys_t physical_addr, flags_t flags);

/**
 * Unmaps a virtual page
 */
void unmap(virt_t virtual_addr);

/**
 * Temporary maps a physical page to a known address
 */
virt_t *temp_map(phys_t *page);

/**
 * Unmaps the temporary page
 */
void temp_unmap();

/**
 * Giving a virtual page address, returns the physical frame mapped in
 */
phys_t virt_to_phys(virt_t addr);

/**
 * Checks if this virtual page is mapped
 */
bool is_mapped(virt_t addr);

/**
 * Allocates a physical frame
 */
phys_t *frame_alloc();

/**
 * Allocates a physical frame and zero it
 */
phys_t *frame_calloc();

/**
 * Frees a frame
 */
void frame_free(phys_t addr);


void link_pde(phys_t *new_dir, virt_t *old_dir, int pd_idx);
void clone_pde(phys_t *new_dir, virt_t *old_dir, int pd_idx);
void free_pde(phys_t *dir, int pd_idx);
void recursively_map_page_directory(dir_t *dir);

/**
 * Clones current directory
 */
dir_t *clone_directory();

/**
 * Frees a directory
 */
void free_directory(dir_t *dir);

/**
 * Global variables
 */
size_t num_pages;       // number of free pages
size_t total_pages;     // number of total pages
dir_t *kernel_dir;      // kernel page directory 
bool pg_on;             // is paging enaled?
phys_t last_free_page;  // last free, unallocated page

#endif
