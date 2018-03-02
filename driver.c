#include "console.h"
#include "pci.h"
#include "driver.h"
#include "ne2000.h"

struct driver *drivers[] = {
    &ne2000_driver,
    NULL,
};

int driver_init()
{
    int i, j;
    struct pci_dev *pci_device;
    struct driver *driver;

    for (i = 0; i < pci_devs.idx; i++) {
        pci_device = &pci_devs.devs[i];
        for (j = 0; drivers[j]; j++) {
            driver = drivers[j];
            if (!driver->inited && driver->init) {
                if (driver->init(driver) < 0) {
                    continue;
                }
            }
            if (driver->probe(pci_device) == 0) {
                driver->attach(pci_device);
            }
        }
    }

    return 0;
}

