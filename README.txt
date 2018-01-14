cOSiris is a small kernel written in C (and some ASM) for i386, for fun.
monolitic, multitasking, protected mode, ring 0,3 userland, small shell, hard disk unix like filesystem

Inspired by linux-0.01, osd, ics-os, james molloy, geekos, unix v6, xv6, osdev site

How to compile it:
In order to compile it, you will need gcc installed on your system; 

How to run it:
You can run cOSiris in an emulator (recomended) bochs, qemu or VirtualBox
 or on real hardware, although I do not recommend it.


- compile - make
- to run it inside qemu - type make run (require qemu-system-i386)
- to run it with gdb attached - type make debug and attach
- to run it inside bochs - type make runb (require bochs bochs-term bochsbios)

- make a floppy image - make fd
	will create fd.img, to write it - dd if=fd.img of=/dev/fd0

Memory info
_____
kernel identity mapping 0x00100000	-> linked on every fork()
USER_STACK_LOW          0x0FFEF000	// actually only 2 pages mapped
USER_STACK_HI           0x0FFFF000	<- cloned on fork
USER_CODE_START_ADDR 	0x10000000	-> cloned on fork, not linked yet
			user code
User heap starts	0x20000000 	-> cloned on fork
User heap max		0x30000000

Kernel heap starts	0xD0000000 	// kernel heap is linked for every processs
Kernel heap max		0xE0000000	// so all dynamic kernel vars can be seen and modified on every process, when switch to ring 0

KERNEL_STACK_HI         0xFF118000 	<- // after moving it up, size: 4 pages. stack is cloned / duplicated for every process
RESV_PAGE		0xFFBFE000
PTABLES_ADDR		0xFFC00000
PDIR_ADDR		0xFFFFF000
kernel stack is cloned/copy for every process.
____


## bochs mmap
size = 0x14, base_addr = 0x00, length = 0x09fc00, type = 0x1		   654336 - free
size = 0x14, base_addr = 0x09fc00, length = 0x0400, type = 0x2		     1024 - EBDA?		                                                                                                _
size = 0x14, base_addr = 0x0e8000, length = 0x018000, type = 0x2	    98304 - BIOS
size = 0x14, base_addr = 0x0100000, length = 0x06f0000, type = 0x1	7,274,496 - free
size = 0x14, base_addr = 0x07f0000, length = 0x010000, type = 0x3	   65,536 - ?
size = 0x14, base_addr = 0x0fffc0000, length = 0x040000, type =0x2	  262,144 - BIOS?


## my old duron 800 mmap
0x00		0x09fc00	0x1
0x09fc00	0x400		0x2
0x0f0000	0x010000	0x2
0x0100000	0x0eef000	0x1
0x0eff0000	0x08000		0x3
0x0eff8000	0x08000		0x4
0x0ffc0000	0x040000	0x2


## my old P1/90Mhz mmap
flags - 07E7
0x00		0x09fc00	0x1
0x010000000	0x01700000	0x1
0c0fffc0000	0x040000	0x2



## my old 486sx/33Mhz
flags - 0x3A7
low 639Kb high 7168Kb

# !!! No memory map structure !!!
