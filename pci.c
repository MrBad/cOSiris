#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "vfs.h"
#include "console.h"
#include "i386.h"
#include "pci.h"

#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

#define PCI_VENDOR_ID 0x0
#define PCI_DEVICE_ID 0x02
#define PCI_CLASS 0x0B
#define PCI_SUBCLASS 0x0A
#define PCI_HEADER_TYPE 0x0E
#define PCI_BAR0 0x10
#define PCI_BAR1 0x14
#define PCI_BAR2 0x18
#define PCI_BAR3 0x1C
#define PCI_BAR4 0x20
#define PCI_BAR5 0x24
#define PCI_ROM_ADDR 0x30
#define PCI_INT_LINE 0x3C

struct pci_devs pci_devs;

char *pci_class_codes[] = {
    "Unclassified device",
    "Mass storage controller",
    "Network controller",
    "Display controller",
    "Multimedia controller",
    "Memory controller",
    "Bridge",
    "Communication controller",
    "Generic system peripheral",
    "Input device controller",
    "Docking station",
    "Processor",
    "Serial bus controller",
    "Wireless controller",
    "Intelligent controller",
    "Satellite communications controller",
    "Encryption controller",
    "Signal processing controller",
    "Processing accelerators",
    "Non-Essential Instrumentation",
    "Coprocessor",
    "Unassigned class",
};
#define PCI_CFG_CMD(bus, slot, func, offs) \
    (0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | (offs & ~3))

static uint8_t pci_cfg_readb(int bus, int slot, int func, int offs)
{
    uint8_t ret;
    outl(PCI_CFG_ADDR, PCI_CFG_CMD(bus, slot, func, offs));
    ret = inb(PCI_CFG_DATA + (offs & 3));
    return ret;
}

static uint16_t pci_cfg_readw(int bus, int slot, int func, int offs)
{
    uint16_t ret;
    outl(PCI_CFG_ADDR, PCI_CFG_CMD(bus, slot, func, offs));
    ret = inw(PCI_CFG_DATA + (offs & 2));
    return ret;
}

static uint32_t pci_cfg_readl(int bus, int slot, int func, int offs)
{
    outl(PCI_CFG_ADDR, PCI_CFG_CMD(bus, slot, func, offs));
    return inl(PCI_CFG_DATA);
}

static void pci_cfg_writel(int bus, int slot, int func, int offs, uint32_t val)
{
    outl(PCI_CFG_ADDR, PCI_CFG_CMD(bus, slot, func, offs));
    outl(PCI_CFG_DATA, val);
}

static uint32_t pci_bar_size(int bus, int slot, int func, int bar)
{
    uint32_t size, mask, addr;
    addr = pci_cfg_readl(bus, slot, func, bar);
    pci_cfg_writel(bus, slot, func, bar, ~0);
    mask = pci_cfg_readl(bus, slot, func, bar);
    size = ~(mask & ~0xF) + 1;
    pci_cfg_writel(bus, slot, func, bar, addr);
    return size;
}

static int pci_scan()
{
    uint16_t bus, slot, func, vendor_id, class_id;
    int bar, i;
    uint32_t addr, low, hi;
    struct pci_dev *p;
    struct bar *b;

    for (bus = 0; bus <= 255; bus++) {
        for (slot = 0; slot < 32; slot++) {
            for (func = 0; func < 8; func++) {
                vendor_id = pci_cfg_readw(bus, slot, func, PCI_VENDOR_ID);
                if (vendor_id == 0xFFFF)
                    continue;
                class_id = pci_cfg_readb(bus, slot, func, PCI_CLASS);
                /* Skip PCI bridge */
                if (class_id == 0x06)
                    continue;
                p = &pci_devs.devs[pci_devs.idx++];
                if (pci_devs.idx == PCI_MAX_DEVS) {
                    kprintf("PCI devs; not enough slots\n");
                    return -1;
                }
                p->bus = bus;
                p->slot = slot;
                p->func = func;
                p->vendor_id = vendor_id;
                p->device_id = pci_cfg_readw(bus, slot, func, PCI_DEVICE_ID);
                p->class = class_id;
                p->subclass = pci_cfg_readb(bus, slot, func, PCI_SUBCLASS);

                for (bar = PCI_BAR0, i = 0; bar <= PCI_BAR5; bar += 4) {
                    addr = pci_cfg_readl(bus, slot, func, bar);
                    if (addr == 0)
                        continue;
                    b = &p->bars[i++];
                    b->type = addr & 0x1;
                    /* IO type BAR - base is a port */
                    if (b->type == PCI_BAR_IO) {
                        b->base = (addr & ~3);
                    }
                    /* Memory type BAR - base is memory */
                    else {
                        b->subtype = (addr & 6) >> 1;
                        /* 64 bit bar type, if base address is mapped into the
                         * first 4 GB, we can use it */
                        if (b->subtype == PCI_BAR_MM64) {
                            low = (addr & ~0xF);
                            hi = pci_cfg_readl(bus, slot, func, bar + 4);
                            if (hi != 0) {
                                kprintf("Cannot use 64 bit bar h:%#x, l:%#x\n",
                                        hi, low);
                                continue;
                            }
                            b->base = low;
                            b->size = pci_bar_size(bus, slot, func, bar);
                            bar += 4;
                        }
                        /* 32 bit bar type */
                        else if (b->subtype == PCI_BAR_MM32) {
                            b->base = (addr & ~0xF);
                            b->size = pci_bar_size(bus, slot, func, bar);
                        } else {
                            kprintf("Unknown mem bar type: %d\n", b->subtype);
                            i--;
                        }
                    }
                }
                p->irq = pci_cfg_readb(bus, slot, func, PCI_INT_LINE);
            }
        }
    }

    return 0;
}

/**
 * Dumps pci_devs.
 * If file_name is defined, write them to file
 * If print > 0, print to console
 */
int pci_dump(char *file_name, int print)
{
    fs_node_t *file = NULL;
    char line[512];
    int offs, n, i, j;
    struct pci_dev *p;
    if (file_name) {
        if (fs_open_namei(file_name, O_WRONLY | O_CREAT, 0666, &file) < 0) {
            kprintf("pci: cannot open %s\n", file_name);
            return -1;
        }
    }
    for (i = 0, offs = 0; i < PCI_MAX_DEVS; i++) {
        p = &pci_devs.devs[i];
        if (!p->vendor_id)
            continue;
        n = snprintf(line, sizeof(line), 
                "%s\n  "
                "vendor_id: %#x, device_id: %#x, "
                "class: %#x, subclass: %#x, irq: %d\n",
                pci_class_codes[p->class],
                p->vendor_id, p->device_id, p->class, p->subclass, p->irq);
        if (print)
            kprintf("%s", line);
        if (file) {
            fs_write(file, offs, n, line);
            offs += n;
        }
        for (j = 0; j < 6; j++) {
            if (p->bars[j].base == 0)
                continue;
            n = snprintf(line, sizeof(line),
                "  bar: %d, type: %s, base: %#x, size: %#x\n",
                j, p->bars[j].type == PCI_BAR_MEM ? "mem":"io",
                p->bars[j].base, p->bars[j].size);
            if (print)
                kprintf("%s", line);
            if (file) {
                fs_write(file, offs, n, line);
                offs += n;
            }
        }
    }
    if (file)
        fs_close(file);

    return 0;
}

int pci_init()
{
    /* In theory vars on bss are clean, but, just in case */
    memset(&pci_devs, 0, sizeof(pci_devs));
    if (pci_scan() < 0) {
        kprintf("Errors in scanning pci\n");
        return -1;
    }
    pci_dump("pci.txt", 1);
    return 0;
}

