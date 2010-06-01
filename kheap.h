#define KHEAP_START				0xE0000000			// addr where the heap starts
#define KHEAP_INIT_SIZE			0x10000				// heap initial size
#define KHEAP_END				0xEFFFF000			// max heap addr
#define KHEAP_MAGIC				0xDEADC0DE			// magic number to check if the header mem block was altered
#define KHEAP_MIN_EXPAND_PAGES	1					// minimum pages to expand in one kheap_expand

typedef struct mem_block_t {
	unsigned int magic;
	unsigned int size;
	unsigned short int is_free;
	struct mem_block_t *prev;
	struct mem_block_t *next;

} mem_block_t;

void *kalloc(unsigned int nbytes);
void kfree(void *ptr);
void kheap_dump();