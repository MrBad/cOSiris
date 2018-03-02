#ifndef _PCI_H
#define _PCI_H
#include <sys/types.h>

#define PCI_MAX_DEVS 32 /* Max number of pci devices that we can handle */

#define PCI_MAX_BARS 6
#define PCI_BAR_MEM 0x0 /* MEM type BAR */
#define PCI_BAR_IO 0x1  /* IO type BAR */
#define PCI_BAR_PREFECHABLE 0x08

#define PCI_BAR_MM32 0  /* 32 bit memory subtype BAR */
#define PCI_BAR_MM64 2  /* 64 bit memory subtype BAR */

struct pci_id {
    uint16_t vendor_id;
    uint16_t device_id;
};

/* Base Address Register representation */
struct bar {
    uint32_t base;      /* Base is memory address or port */
    uint32_t size;      /* Size of memory address*/
    uint8_t type;       /* Type, PCI_BAR_IO | PCI_BAR_MEM */
    uint8_t subtype;    /* Subtype for PCI_BAR_MEM - MM32 - MM64*/
};

struct pci_dev {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t class;
    uint16_t subclass;
    uint16_t vendor_id;
    uint16_t device_id;
    struct bar bars[6];
    uint8_t irq;
    void *priv; /* Driver instance private data */
};

struct pci_devs {
    struct pci_dev devs[PCI_MAX_DEVS];
    int idx;
};

extern struct pci_devs pci_devs;

int pci_dump(char *file_name, int print);
int pci_init();

#endif

