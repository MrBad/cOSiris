/**
 * Block memory manager
 * 
 * Note: 
 * A frame will refer to a PAGE_SIZE, PAGE_SIZE aligned block of
 *   physical memory.
 * A page will refer to a virtual PAGE_SIZE, PAGE_SIZE aligned 
 *   virtual memory.
 */
#include <sys/types.h>	// types //
#include <string.h>		// memset //
#include "i386.h"       // invlpg
#include "console.h"	// kprint, panic//
#include "crt.h"        // CRT_ADDR
#include "isr.h" 		// iregs //
#include "multiboot.h"	// multiboot_header //
#include "kernel.h" 	// kinfo //
#include "kheap.h"		// HEAP defs //
#include "assert.h"		// KASSERT /
#include "task.h"
#include "mem.h"

extern heap_t * kernel_heap;

/**
 * Is paging active?
 */
bool paging_on()
{
    return pg_on;
}

/**
 * Page fault isr
 */
int page_fault(struct iregs *r)
{
    static int fault_counter = 0;
    uint32_t addr = get_fault_addr();
    int dir_idx = (addr / PAGE_SIZE) / 1024;
    int tbl_idx = (addr / PAGE_SIZE) % 1024;
    fault_counter++;
    if (fault_counter > 10) {
        panic("Too many faults");
    }
    kprintf("_____\n"
            "FAULT: %s addr: 0x%X, stack: 0x%X\n"
            , r->err_code & P_PRESENT ? "page violation" : "page not present", 
            addr, get_esp());
    if (current_task) {
        kprintf("task %s pid %u\n", current_task->name, current_task->pid);
    }
    if (USER_STACK_HI <= addr && addr < USER_CODE_START_ADDR) {
        kprintf("User stack overflow!\n");
    }
    if (USER_STACK_LOW < addr && addr < USER_STACK_HI) {
        kprintf("In user stack! "
                "Automatically user stack grow not available yet.\n");
    }
    kprintf("Virtual page: 0x%p, dir_idx: %u, tbl_idx: %u\n", 
        dir_idx * 1024 * PAGE_SIZE + tbl_idx * PAGE_SIZE, dir_idx, tbl_idx);

    kprintf("From eip: 0x%p, ", r->eip);
    kprintf("%s, ", r->err_code & P_READ_WRITE ? "write" : "read", addr);
    kprintf("%s mode\n", r->err_code & P_USER ? "user" : "kernel");
    kprintf("Page is %s\n", is_mapped(addr & ~0xFFF) ? "mapped":"not mapped");
    if (kernel_heap->start_addr && HEAP_START < addr && addr < HEAP_END) {
        kprintf("In heap\n");
        heap_dump(kernel_heap);
    }
    kprintf("Paging was %s\n", paging_on() ? "ON" : "OFF");
    ps();
    if (r->err_code & P_USER) {
        kprintf("_____\nGoing to terminate pid: %d\n", current_task->pid);
        task_exit(-1);
    } 
    return false;
}

/**
 * Allocates a free frame.
 * Returns address of the frame.
 * Note: each frame will point to the next free frame available.
 */
phys_t *frame_alloc()
{
    phys_t *addr;
    KASSERT(last_free_page != 0);
    addr = (phys_t *)last_free_page;
    if (paging_on()) {
        map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
        last_free_page = *((phys_t *)RESV_PAGE);
        *((phys_t *)RESV_PAGE) = 0;
        unmap(RESV_PAGE);
    } else {
        last_free_page = *addr;
        *addr = 0; // unlink next //
    }
    num_pages--;
    
    return addr;
}

/**
 * Allocates and erase a frame.
 * Returns the address of allocated frame.
 */
phys_t *frame_calloc()
{
    phys_t *addr;
    addr = frame_alloc();
    if (paging_on()) {
        map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
        memset((void *)RESV_PAGE, 0, PAGE_SIZE);
        unmap(RESV_PAGE);
    } else {
        memset(addr, 0, PAGE_SIZE);
    }

    return addr;
}

/**
 * Frees the physical frame located at address addr.
 * Note: each free frame will point to the next available frame.
 */
void frame_free(phys_t addr)
{
    KASSERT(!(addr & 0xFFF));
    if (paging_on()){
        map(RESV_PAGE, (phys_t)addr, P_PRESENT|P_READ_WRITE);
        *((phys_t *)RESV_PAGE) = last_free_page;
        unmap(RESV_PAGE);
    } else {
        *((phys_t *)addr) = last_free_page;
    }
    last_free_page = addr;
    num_pages++;
}

/**
 * Reserve (mark as free frames) the physical block of memory starting
 *   at address addr having the length length
 * This function will skip marking as free the blocks like video memory block 
 *   or kernel code/data block.
 */
static void reserve_region(phys_t addr, size_t length)
{
    phys_t	ptr;
#if 0
    phys_t	initrd_location = 0,
            initrd_end = 0;
    if (mb && mb->mods_count > 0) {
        initrd_location = *((unsigned int *) mb->mods_addr);
        initrd_end = *(unsigned int *) (mb->mods_addr + 4);
        initrd_end = round_page_up(initrd_end);
    }
#endif

    for (ptr = addr; ptr - addr < length; ptr+=PAGE_SIZE) {
        // We will not use first frame, to be able to catch errors //
        if (ptr == 0) {
            *((phys_t *) addr) = 0;
            continue;
        }
        // Skip frames from vga start addr to end of the kernel //
        if (ptr >= CRT_ADDR && ptr <= kinfo.end) {
            continue;
        }
#if 0
        // Skip frames that belongs to initrd section //
        if(initrd_location) {
            if(ptr >= initrd_location && ptr <= initrd_end) {
                continue;
            }
        }
#endif
        frame_free(ptr);
    }
}

/**
 * Reserve (mark as free) the usable system memory, passed in kinfo
 */
static void reserve_memory()
{
    int i, len = sizeof(kinfo.minfo) / sizeof(*kinfo.minfo);
    struct minfo *minfo;
    for (i = 0; i < len; i++) {
        minfo = kinfo.minfo + i;
        if (minfo->type == MEM_USABLE) {
            reserve_region(minfo->base, minfo->length);
        }
    }
}

/**
 * Maps recursively the page directory
 */
static void recursively_map_page_directory(dir_t *dir)
{
    if (paging_on()) {
        map(RESV_PAGE, (phys_t) dir, P_PRESENT | P_READ_WRITE);
        ((virt_t *) RESV_PAGE)[1023] = (phys_t)dir | P_PRESENT | P_READ_WRITE;
        unmap(RESV_PAGE);
    } else {
        dir[1023] = (phys_t) dir | P_PRESENT | P_READ_WRITE;
    }
}

/**
 * Identity maps a page, so physical address == virtual address
 * This will be called only when paging is not yet enabled.
 */
static void identity_map_page(dir_t *dir, phys_t addr)
{
    phys_t *table;
    int dir_idx, tbl_idx;
    dir_idx = (addr / PAGE_SIZE) / 1024;
    tbl_idx = (addr / PAGE_SIZE) % 1024;
    if (!(dir[dir_idx] & P_PRESENT)) {
        dir[dir_idx] = (uint32_t) frame_calloc() | P_PRESENT | P_READ_WRITE;
    }
    table = (phys_t *)(dir[dir_idx] & 0xFFFFF000);
    table[tbl_idx] = addr | P_PRESENT | P_READ_WRITE;
}

/**
 * Identity maps regions of the memory:
 *   - video memory -> 1MB
 *   - 1MB, where kernel starts -> kernel end
 *   - allocate a frame for RESV_PAGE and mark it as present
 */
static void identity_map_kernel(dir_t *dir)
{
    phys_t addr;

    // Identity maps pages from VGA addr to kernel end //
    for (addr = CRT_ADDR; addr <= kinfo.end; addr += PAGE_SIZE) {
        identity_map_page(dir, addr);
    }

#if 0
    // identity maps from initrd_location to initrd_end -> used by initrd, later
    // maibe we will move it up into the memory and not ident,
    if (mb->mods_count > 0) {
        phys_t	initrd_location = 0, initrd_end = 0;
        initrd_location = *((unsigned int *) mb->mods_addr);
        initrd_end = *(unsigned int *) (mb->mods_addr + 4);
        initrd_end = round_page_up(initrd_end);
        for(addr = initrd_location; addr <= initrd_end; addr += PAGE_SIZE) {
            identity_map_page(dir, addr);
        }
    }
#endif

    // Creates the table for reserved page //
    int dir_idx;
    dir_idx = (RESV_PAGE/PAGE_SIZE) / 1024;
    dir[dir_idx] = (uint32_t)frame_calloc() | P_PRESENT | P_READ_WRITE;
}

/**
 * Dumps the directory to screen
 */
void dump_dir()
{
    int dir_idx, tbl_idx;
    dir_t *dir = (dir_t *)PDIR_ADDR;
    virt_t *table;
    for (dir_idx = 0; dir_idx < 1024; dir_idx++)
    {
        if (dir[dir_idx] & P_PRESENT) {
            kprintf("Dir: %d\n", dir_idx);
            for (tbl_idx=0; tbl_idx < 1024; tbl_idx++) {
                table = (uint32_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
                if (table[tbl_idx] & P_PRESENT) {
                    kprintf("%d ",tbl_idx);
                }
            }
            kprintf("\n");
        }
    }
}

/**
 * Checks if virtual page located at addr is mapped
 */
bool is_mapped(virt_t addr)
{
    dir_t *dir = (dir_t *) PDIR_ADDR;
    virt_t *table;
    if (addr & 0xFFF) {
        kprintf("is_mapped(): Not aligned: %p\n", addr);
    }
    int dir_idx = (addr / PAGE_SIZE) / 1024;
    int tbl_idx = (addr / PAGE_SIZE) % 1024;
    if (!(dir[dir_idx] & P_PRESENT)) {
        return false;
    }
    table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
    if (!(table[tbl_idx] & P_PRESENT)) {
        return false;
    }

    return true;
}

/**
 * Gets physical frame mapped to virtual address addr
 */
phys_t virt_to_phys(virt_t addr)
{
    virt_t *table;
    int dir_idx = (addr / PAGE_SIZE) / 1024;
    int tbl_idx = (addr / PAGE_SIZE) % 1024;
    KASSERT(!(addr & 0xFFF));
    if (!is_mapped(addr)) {
        panic("%p is not mapped\n", addr);
    }
    table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
    
    return table[tbl_idx] & 0xFFFFF000;
}

/**
 * Unmaps a virtual address
 */
void unmap(virt_t virtual_addr)
{
    map(virtual_addr, 0, 0);
}

/**
 * Maps a frame located at physical_addr to a page at virtual_addr, with
 * flags
 */
void map(virt_t virtual_addr, phys_t physical_addr, flags_t flags)
{
    dir_t *dir = (dir_t *) PDIR_ADDR;
    virt_t *table;
    int dir_idx = (virtual_addr / PAGE_SIZE) / 1024;
    int tbl_idx = (virtual_addr / PAGE_SIZE) % 1024;

    table = (virt_t *)(PTABLES_ADDR + dir_idx * PAGE_SIZE);

    if (! dir[dir_idx] & P_PRESENT) {
        dir[dir_idx] = (phys_t) frame_alloc() | flags;
        invlpg((virt_t) table);
        memset(table, 0, PAGE_SIZE);
    }
    table[tbl_idx] = physical_addr | flags;
    invlpg(virtual_addr);
}

/**
 * Temporary maps a physical frame to a known virtual address so we can 
 * change the frame contents when paging is on
 */
virt_t *temp_map(phys_t *page)
{
    map(RESV_PAGE, (phys_t)page, P_PRESENT|P_READ_WRITE);
    return (virt_t *)RESV_PAGE;
}

/**
 * Unmaps known virtual addr
 */
void temp_unmap()
{
    unmap(RESV_PAGE);
}

/**
 * Clones current address space and returns a new dir
 *   this is how fork() is done :)
 */
dir_t *clone_directory()
{
    static int iter = 0;
    unsigned int dir_idx, tbl_idx;
    dir_t *curr_dir = (dir_t *) PDIR_ADDR;      // current directory table
    dir_t *new_dir = (dir_t *) frame_calloc();  // new directory table, cloned
    phys_t *new_table, *frame;
    virt_t *table, *addr, *from_addr;

    cli();
    for (dir_idx = 0; dir_idx < 1023; dir_idx++) {
        if (curr_dir[dir_idx] & P_PRESENT) {
            /**
             * All programs will "see" the same linked zones of memory
             * These zones are: video memory, kernel code, kernel static vars,
             * kernel heap, but not kernel stack
             */
            if ((dir_idx < ((USER_STACK_LOW)/1024/PAGE_SIZE))
                || (
                    dir_idx >= (HEAP_START/1024/PAGE_SIZE) 
                    && dir_idx < (HEAP_END/1024/PAGE_SIZE)
                    )
            ) {
                addr = temp_map(new_dir);
                addr[dir_idx] = curr_dir[dir_idx];
                temp_unmap();
                // kprintf("Link dir: %p\n", (dir_idx * 1024 * PAGE_SIZE));
            }
            /**
             * The reset of the memory will be cloned (assign new mem and copy)
             * This will be used in fork(), when every new process will "see" 
             * it's own copy of program stack, it's own copy of code and data, 
             * and a copy of kernel stack
             * TODO - use P_USER on USER_STACK, P_READ only on code.
             */
            else {
                int dir_flags = curr_dir[dir_idx] & 0xFFF;
                new_table = frame_calloc();
                addr = temp_map(new_dir);
                addr[dir_idx] = (phys_t)new_table | dir_flags;
                temp_unmap();

                table = (virt_t *) (PTABLES_ADDR + dir_idx * PAGE_SIZE);
                for (tbl_idx = 0; tbl_idx < 1024; tbl_idx++) {
                    if (table[tbl_idx] & P_PRESENT) {

                        int table_flags = table[tbl_idx] & 0xFFF;
                        // make a copy :) //
                        frame = (phys_t *)frame_calloc();
                        from_addr = (virt_t *)(dir_idx * 1024 * PAGE_SIZE 
                                + tbl_idx * PAGE_SIZE);
                        //kprintf("copy phys: %p->%p, flags: 0x%3x\n", 
                        //          from_addr, frame, table_flags);
                        addr = temp_map(frame);
                        memcpy(addr, from_addr, PAGE_SIZE);
                        temp_unmap();

                        addr = temp_map(new_table);
                        addr[tbl_idx] = (phys_t)frame | table_flags;
                        temp_unmap();
                    }
                }
            }
        }
    }
    recursively_map_page_directory(new_dir);
    iter++;
    sti();
    
    return new_dir;
}

/**
 * Frees memory for a cloned directory
 */
void free_directory(dir_t *dir)
{
    // cli();
    virt_t *addr;
    phys_t pde, pte;
    unsigned int dir_idx, tbl_idx;

    // kprintf("Free directory: %p, curr: %p\n", dir, virt_to_phys(PDIR_ADDR));

    // Don't Try to free current working directory (self freeing)
    KASSERT((virt_to_phys(PDIR_ADDR) != (phys_t)dir)); 

    for (dir_idx = 0; dir_idx < 1023; dir_idx++) {
        addr = temp_map(dir);
        pde = addr[dir_idx];
        temp_unmap(dir);
        if (pde & P_PRESENT) {
            // Skip linked pages, common to all processes
            if ((dir_idx < ((USER_STACK_LOW)/1024/PAGE_SIZE)) 
                || (
                    dir_idx >= (HEAP_START/1024/PAGE_SIZE) 
                    && dir_idx < (HEAP_END/1024/PAGE_SIZE)
                )
            ) {
                continue;
            }
            for (tbl_idx = 0; tbl_idx < 1024; tbl_idx++) {
                addr = temp_map((phys_t *)(pde & 0xFFFF000));
                pte = addr[tbl_idx];
                temp_unmap();
                if (pte & P_PRESENT) {
                    frame_free(pte & 0xFFFF000); // free (data|code) frame
                }
            }
            frame_free(pde & 0xFFFF000); // free frame assigned to pde
        }
    }
    frame_free((phys_t)dir); // free directory
    // sti();
}

/**
 * Moving stack up in memory (KERNEL_STACK_HI)
 * so clone_directory() will clone it instead of linking it
 * Also it patches the stack values that reffers to stack, so they point
 * to new location
 * TODO: free old stack frames available and update kinfo structure.
 */
static void move_stack_up()
{
    KASSERT(pg_on);
    uint32_t i;
    // alloc and map the pages for the new stack //
    for(i = kinfo.stack_size / PAGE_SIZE; i; i--) {
        map(KERNEL_STACK_HI - PAGE_SIZE * i, (uint32_t) frame_calloc(), 
                P_PRESENT | P_READ_WRITE);
    }
    // jump to assembly, copy the stack up and patch it's values
    patch_stack(KERNEL_STACK_HI);
}

/**
 * sbrk, internal function.
 * Ugly, but...
 * Calling sbrk(0) gives the current address of program break.
 * Calling sbrk(x) with a positive value increments brk by x bytes, 
 *  as a result allocating memory.
 * Calling sbrk(-x) with a negative value decrements brk by x bytes, 
 *  as a result releasing memory.
 *  On failure, sbrk() returns (void*) -1.
 */
static void *_sbrk(heap_t *heap, int increment)
{
    uint32_t ret, iterations, i, frame;
    ret = heap->end_addr; // save current end_addr //

    if (increment == 0) {
        ret = heap->end_addr;
        goto clean;
    }
    if (increment > 0) {
        iterations = (increment / PAGE_SIZE);
        if (increment % PAGE_SIZE) 
            iterations++; // round up;
        for (i = 0; i < iterations; i++) {
            if (!(frame = (uint32_t) frame_alloc())) {
                ret = -1;
                goto clean;
            }
            map(heap->end_addr, frame, P_PRESENT | P_READ_WRITE |
                    (heap->supervisor ? 0 : P_USER));
            heap->end_addr += PAGE_SIZE;
            if (heap->end_addr >= heap->max_addr) {
                ret = -1;
                goto clean;
            }
        }
    }
    else {
        increment = -increment;
        iterations = (increment / PAGE_SIZE);
        if (increment % PAGE_SIZE) 
            iterations++; // round up
        for (i = 0; i < iterations; i++) {
            heap->end_addr -= PAGE_SIZE;
            if (heap->end_addr < heap->start_addr) {
                ret = -1;
                goto clean;
            }
            frame_free(virt_to_phys(heap->end_addr));
            unmap(heap->end_addr);
        }
    }

clean:
    return (void *) ret;
}

/**
 * Ok, trick is that sbrk will be called directly when compiling libc in 
 * kernel mode, but in usermode, sys_sbrk will be called instead.
 * Based on that, we switch the heap where we want to sbrk 
 * and use same malloc in both places.
 */
void *sbrk(int increment)
{
    return _sbrk(kernel_heap, increment);
}

void *sys_sbrk(int increment)
{
    return _sbrk(current_task->heap, increment);
}

/**
 * Memory initialization and switch to paging protected mode
 */
void mem_init()
{
    num_pages = 0;
    total_pages = 0;
    kernel_dir = NULL;
    pg_on = false;
    last_free_page = 0;

    reserve_memory();
    total_pages = num_pages;
    kprintf("Available memory: %d pages, %d MB\n", 
            num_pages, num_pages * 4/1024);
    kernel_dir = frame_calloc();
    identity_map_kernel(kernel_dir);
    recursively_map_page_directory(kernel_dir);
    //switch_page_directory(kernel_dir);
    switch_pd(kernel_dir);
    // we are now in paging, protected mode //
    pg_on = true;
    move_stack_up();
    kprintf("Current paging directory is at: 0x%X\n", virt_to_phys(PDIR_ADDR));
}
