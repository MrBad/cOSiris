#define PAGE_SIZE       0x1000 // 4Kb

#define P_PRESENT                       1<<0    // pagina este in memoria fizica?
#define P_READ_WRITE            1<<1    // isset ? read/write : read
#define P_USER                          1<<2    // isset ? acess all : access supervisor
#define P_WRITE_THROUGH         1<<3    // daca e setat, write-through caching enabled else write-back enabled
#define P_CACHE_DISABLED        1<<4    // valabil pt page directory - if set, page will not be cached
#define P_ACCESSED                      1<<5    // a fost accesata
#define P_DIRTY                         1<<6    // valabil pt page tables - if set, page was accessed
#define P_SIZE                          1<<7    // valabil pt page directoru - pagini de 4K sau 4M
#define P_GLOBAL                        1<<8    // if set, prevents TLB from updating the cache if CR3 is reset

extern unsigned int *kernel_dir;
extern void switch_page_directory(unsigned int *page_dir);
void mem_init();
extern void flush_tlb();
unsigned int pop_frame();
void push_frame(unsigned int frame_addr);
void *ikalloc(unsigned int size, int align);
//void map_linear_to_any(unsigned int *dir, unsigned int linear_addr, unsigned int flags);
void map_linear_to_physical(unsigned int linear_addr, unsigned int physical_addr, unsigned int flags);
//void map_range_any(unsigned int *dir, unsigned int linear_start, unsigned int linear_end, unsigned int flags);
//void map_range_physical(unsigned int *dir, unsigned int linear_start, unsigned int linear_end, unsigned int physical_start, unsigned int flags);
