cOSiris is a small Unix like operating system written in C (and some ASM) 
for Intel X86, for fun. 
It is build from scratch and right now it has: a monolitic, multitasking,
protected mode kernel, few userland programs, a small shell (cosh), 
an Unix like file system (cofs), and a WIP network stack.
There is also a Linux module that i've wrote to mount this type of custom 
file system available here https://github.com/MrBad/cofs and also 
as a git submodule.
It can run on emulators or on real hardware, but with limitations.
Few videos are available here: 
https://www.youtube.com/playlist?list=PL_E8HVuR4axHayR5EV8iY-Rp_AkfYrd3V
---

0. History because without it, we are lost.
Back in university i always wanted to understand the Intel CPU (aka i386).
I started then (around 2004) OSiris (https://github.com/MrBad/OSiris), in ASM,
but i never gone too far, because the lack of time, resources, switching to 
web technologies, etc.
In 2010 or so this idea came back into my mind, but by then, ASM was a little 
bit out of my taste, being a little bit hard for me to code in it.
So, because my first love was C, i decided to start all over again, and use it.
And cOSiris was born or at least the idea.

---

1. Download, run and compile.

How to download it:
  git clone https://github.com/MrBad/cOSiris
  cd cOSiris
  git submodule init
  git submodule update

How to compile it:
In order to compile it, you will need gcc installed on your system. There is
no need yet to recompile gcc, because of ffreestanding flag.
To compile it, type 'make run' in the root of the project.

How to run it:
You can run cOSiris in an emulator or on real hardware. 
Supported emulators right now are: bochs, qemu and VirtualBox.
You can also run it on real hardware (I tested it on ThinkPad x220), 
although I do not recommend it, do it on your own risk.

To run it inside qemu, type 'make run'. The kernel will be compiled, disk image
generated, grub installed, binary files copied to /, and you will be attached 
to the kernel via a serial line console.

To run it inside qemu, with debugging, type 'make debug'. 
On a second console, in the root of the project, type 'gdb kernel.bin' 
to attach to the kernel. 
Inside gdb session, type 'target remote:1234' and go for it and patch it :)
You can also use a gdbinit file (there is a model inside the sources)
For debugging with gdb, I recommend a very nice .gdbinit file, located at
https://github.com/cyrus-and/gdb-dashboard

To run it inside qemu, and also showing qemu's main window, type 'make rung'

To run it inside bochs, type 'make runb'. This will make use of grub and 
possible more multiboot options will be presented (vbe).

To run it inside VirtualBox, there are few more steps to go.
Type 'make diskimg' to generate hdd.img disk image.
Type 'VBoxManage convertdd hdd.img sda.vdi --format VDI' to generate a 
VirtualBox disk image. Start VirtualBox, create a new VM instance, for 'Name
and operating system' use Name: cOSiris, Type: Other, Version: Other/Unknown.
RAM: use default, cOSiris should run on 4MB RAM. Hard drive - Use and existing
virtual hard drive file - browse. Select sda.vdi. Enjoy.

How to run it on real hardware.
I DO NOT recommend this, because the software can be unstable and you can loose
your disk data in theory, but let's say you know what you are doing, you 
back up your disk data, or you use an old computer. Because grub does not 
understand cofs file system not the kernel knows about ext2,
will need 2 partitions, one for boot, where grub and kernel.bin is installed 
and one for cofs file system, for cOSiris flat binaries. 
Because hd.c is using LBA 28 right now, it means there is a limitation on 
where and what size the partitions will be. Kernel will not understand 
partitions over 137GB (2^28*512 bytes), so, both partitions
should be inside this limit. 
For partitioning, you can have a look at script util/mkdisk.sh.
Let's say the disk you want to play with of is /dev/sde. Adjust this device 
to reflect your need.

First, compile the kernel:
  make
Partition it with fdisk:
  sudo fdisk /dev/sde
    n -> new primary partition
    p -> primary
    1 -> first partition, we will install grub and kernel.bin on it
    2048 -> reserve 2048*512 = 1MB
    204800 -> we want 100MB for it
    a -> make it bootable
    p -> print partitions
    n -> new partition
    p -> primary
    2 -> second partition, we will use it for cofs
    204801 -> start of it
    enter -> use default
    p -> print partitions
    w -> write and exit
Verify it:
  sudo fdisk -l /dev/sde

Alternatively you can shrink a Linux partition and use the Linux installed 
grub.

Format first partition for ext2:
  sudo mkfs.ext2 /dev/sde1
Mount it, create directories:
  sudo mount /dev/sde1 /mnt
  sudo mkdir /mnt/grub
  sudo cp kernel.bin /mnt/cosiris.bin
Install grub on it:
  sudo grub-install --root-directory=/mnt --boot-directory=/mnt  --no-floppy \
  --modules="normal part_msdos ext2 multiboot configfile biosdisk" /dev/sde
Add new entry inside /mnt/grub/grub.cfg (create file if does not exist):
  set timeout=3
  set default=0
  menuentry "cOSiris" {
    multiboot /cosiris.bin
    boot
  }
Unmount:
  sudo umount /mnt

Format second partition to cofs and install cOSiris binaries:
  compile cofs - cd util/cofs; make; cd ../../
  util/cofs/mkfs /dev/sde2 bin/init bin/cosh bin/test_malloc bin/test_sbrk \
    bin/test_fork bin/cat bin/mkdir README.txt \
    bin/ls bin/test_write bin/truncate bin/test_append \
    bin/rm bin/tdup bin/pwd bin/tpipe bin/ps bin/cdc bin/reset bin/echo \
    bin/cp bin/mv bin/ln

Reboot, make sure your BIOS is set to use compatibility and not AHCI for the 
disk.

---

2. Virtual memory.

-------------------------------------------------------------------------------
| cOSiris virtual memory map table                                            |
-------------------------------------------------------------------------------
| Start          | End           | Obs.                                       |
-------------------------------------------------------------------------------
| 0x00000000     | 0x00000FFF    | Unused, keep to track null pointers        |
-------------------------------------------------------------------------------
| 0x00001000     | kinfo.end     | Identity mapped video memory and kernel.   |
|                |               |  linked on fork()                          |
-------------------------------------------------------------------------------
| unmapped                                                                    |
-------------------------------------------------------------------------------
| 0x0FFEF000     | 0x0FFFF000    | User stack, cloned on fork()               |
| USER_STACK_LOW | USER_STACK_HI |  fork()                                    |
-------------------------------------------------------------------------------
| 0x0FFFF000     | 0x10000000    | Unused, to track user stack overflows      |
-------------------------------------------------------------------------------
| 0x10000000     | ...           | USER_CODE_START_ADDR, cloned on fork()     |
-------------------------------------------------------------------------------
| 0x20000000     | 0x30000000    | User heap (sys_sbrk, malloc),              |
| UHEAP_START    | UHEAP_END     |  cloned on fork()                          |
-------------------------------------------------------------------------------
| 0xA0000000     | 0xB0000000    |  mmio_remap()                              |
| MMIO_START     | MMIO_END      |                                            |
-------------------------------------------------------------------------------
| 0xD0000000     | 0xE0000000    | Kernel Heap (kernel sbrk, kernel malloc)   |
| HEAP_START     | HEAP_END      |  linked on fork(), so every process "sees" |
|                |               |  same kernel variables.                    |
-------------------------------------------------------------------------------
| -STACKSIZE     | 0xFF118000    | KERNEL_STACK_HI - kernel stack, cloned on  |
|                |               |  fork()                                    |
-------------------------------------------------------------------------------
| 0xFFBFE000     | 0xFFBFF000    | RESV_PAGE, reserved page, cloned on fork() |
|                |               | used in paging manipulation. No need clone |
-------------------------------------------------------------------------------
| 0xFFC00000     | 0xFFFFF000    | PTABLES_ADDR, recursived mapped tables     |
| 0xFFFFF000     | 0xFFFFFFFF    | PDIR_ADDR, recursived mapped dir           |
|                |               |  every process will see it's own page dir  |
-------------------------------------------------------------------------------


