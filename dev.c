#include <stdio.h>
#include <sys/stat.h>
#include "console.h"
#include "tty.h"
#include "vfs.h"
#include "dev.h"
#include "kdbg.h"

open_type_t devices[16] = {
    [4] = tty_open,
    [5] = alt_tty_open,
};

void dev_init()
{
    fs_node_t *dev, *node;
    char buf[512];
    int i;

    if (!(dev = fs_namei("/dev"))) {
        if (!(dev = fs_mkdir(fs_root, "dev", 0755)))
            panic("cannot create /dev\n");
    }
    if (!(node = fs_finddir(dev, "console")))
        node = fs_mknod(dev, "console", 0666, FS_CHARDEVICE | makedev(5, 1));
    if (!node)
        panic("cannot create console\n");

    for (i = 0; i < NTTY + NTTYS; i++) {
#ifdef KDBG
        if (i == KDBG_TTY)
            continue;
#endif
        if (i < NTTY)
            snprintf(buf, sizeof(buf), "tty%d", i);
        else
            snprintf(buf, sizeof(buf), "ttyS%d", i - NTTY);

        if (!(node = fs_finddir(dev, buf)))
            node = fs_mknod(dev, buf, 0666, FS_CHARDEVICE | makedev(4, i));
        if (!node)
            panic("cannot create %s\n", buf);
        fs_close(node);
    }
}

void dev_open(fs_node_t *node, unsigned int flags)
{
    // Check device type and attach the driver.
    if (S_ISCHR(node->type)) {
        if (devices[node->major])
            return devices[node->major](node, flags);
        else
            kprintf("Cannot find any open function for dev major: %d\n",
                    node->major);
    }
}

