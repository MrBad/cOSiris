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

//typedef struct page {
//	unsigned int present    : 1;   // Page present in memory
//	unsigned int writable   : 1;   // Read-only if clear, readwrite if set
//	unsigned int user       : 1;   // Supervisor level only if clear
//	unsigned int accessed   : 1;   // Has the page been accessed since last refresh?
//	unsigned int dirty      : 1;   // Has the page been written to since last refresh?
//	unsigned int unused     : 7;   // Amalgamation of unused and reserved bits
//	unsigned int frame      : 20;  // Frame address (shifted right 12 bits)
//} page_t;
//
//typedef struct page_table {
//	page_t pages[1024];
//} page_table_t;
//
//typedef struct page_directory {
//	page_table_t *tables[1024];
//	unsigned int tablesPhysical[1024];
//} page_directory_t;

extern void switch_page_directory(unsigned int *dir);
extern void flush_tlb();

//extern void *kmalloc_ptr;

void *sbrk(unsigned int increment);
//extern void *kmalloc(unsigned int size, int align, unsigned *phys);
void map(unsigned int virt_addr, unsigned int phys_addr, unsigned short int flags);
int is_mapped(unsigned int virtual_addr);
void mem_init();