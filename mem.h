#ifndef _MEM_H
#define _MEM_H

#include "multiboot.h"
#include <types.h>

//#include "multiboot.h"
#define PAGE_SIZE 4096

// flagurile unei tabele //
// comune pt directory si page tables
#define P_PRESENT                       1<<0    // pagina este in memoria fizica?
#define P_READ_WRITE                    1<<1    // isset ? read/write : read
#define P_USER                          1<<2    // isset ? acess all : access supervisor
#define P_WRITE_THROUGH                 1<<3    // daca e setat, write-through caching enabled else write-back enabled
#define P_CACHE_DISABLED                1<<4    // valabil pt page directory - if set, page will not be cached
#define P_ACCESSED                      1<<5    // a fost accesata
#define P_DIRTY                         1<<6    // valabil pt page tables - if set, page was accessed
#define P_SIZE                          1<<7    // valabil pt page directoru - pagini de 4K sau 4M
#define P_GLOBAL                        1<<8    // if set, prevents TLB from updating the cache if CR3 is reset


typedef uint32_t phys_t;
typedef uint32_t virt_t;
typedef uint32_t dir_t;
typedef uint16_t flags_t;

size_t num_pages;
size_t total_pages;

dir_t *kernel_dir;




// #define KERNEL_STACK_LOW	0xFFBED000ul	// 10000 stack
// #define KERNEL_STACK_HI		0xFFBFD000ul
#define USER_CODE_START_ADDR 0x10000000
#define USER_STACK_LOW		USER_STACK_HI - 10 * PAGE_SIZE
#define USER_STACK_HI		USER_CODE_START_ADDR - PAGE_SIZE
#define KERNEL_STACK_HI		0xFF118000ul
#define RESV_PAGE			0xFFBFE000ul
#define PTABLES_ADDR 		0xFFC00000ul
#define PDIR_ADDR			0xFFFFF000ul

extern void switch_pd(dir_t *dir);
extern void flush_tlb();

//extern void *kmalloc_ptr;

void *sbrk(unsigned int increment);
//extern void *kmalloc(unsigned int size, int align, unsigned *phys);
// void map(unsigned int virt_addr, unsigned int phys_addr, unsigned short int flags);
// int is_mapped(unsigned int virtual_addr);
void map(virt_t virtual_addr, phys_t physical_addr, flags_t flags);
void unmap(virt_t virtual_addr);
virt_t *temp_map(phys_t *page);
void temp_unmap();
void mem_init(multiboot_header *mb);
phys_t virt_to_phys(virt_t addr);
bool is_mapped(virt_t addr);

phys_t *frame_alloc();
phys_t *frame_calloc();
void frame_free(phys_t addr);
dir_t *clone_directory();
void switch_page_directory(dir_t *dir);


#endif
