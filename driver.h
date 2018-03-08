#ifndef _DRIVER_H
#define _DRIVER_H

struct driver {
    const char *name;               /* Driver name */
    bool inited;                    /* Was this driver initialized? */
    int (*init)(struct driver *);   /* Called once, when driver starts */
    int (*probe)(struct pci_dev *); /* Probes if a device is supported by this driver*/
    int (*attach)(struct pci_dev *);/* Creates an instance of the driver and attach */
};

int driver_init();

#endif
