CC = gcc
CFLAGS = -g -Wall -W -nostdlib -fno-builtin -ffreestanding -c -I include
ASM = nasm
ASMFLAGS = -g -f elf
LD = ld
RM = rm -f

BOOTFLAGS = -f bin
LDFLAGS	= -g -T lscript.txt -Map System.map

OBJS = x86.o console.o kernel.o startup.o multiboot.o gdt.o idt.o isr.o \
		irq.o timer.o mem.o libc.a

bzImage: all
	objdump --source kernel.bin > kernel.lst
	objcopy --only-keep-debug kernel.bin kernel.sym
	nm kernel.bin | sort > kernel.SYM
	strip -s kernel.bin
	gzip -c -9 kernel.bin > kernel

all: $(OBJS)
	$(LD) $(LDFLAGS)

libc.a: lib/Makefile
	make -C lib/ -f Makefile

startup.o: startup.asm
	$(ASM) $(ASMFLAGS) startup.asm

kernel.o:
	$(CC) $(CFLAGS) -o kernel.o kernel.c

console.o:
	$(CC) $(CFLAGS) -o console.o console.c

multiboot.o:
	$(CC) $(CFLAGS) -o multiboot.o multiboot.c

gdt.o:
	$(CC) $(CFLAGS) -o gdt.o gdt.c

idt.o:
	$(CC) $(CFLAGS) -o idt.o idt.c

isr.o:
	$(CC) $(CFLAGS) -o isr.o isr.c

irq.o:
	$(CC) $(CFLAGS) -o irq.o irq.c

x86.o:
	$(CC) $(CFLAGS) -o x86.o x86.c

timer.o:
	$(CC) $(CFLAGS) -o timer.o timer.c

mem.o:
	$(CC) $(CFLAGS) -o mem.o mem.c



clean:
	$(RM) $(OBJS) fd.img kernel kernel.lst kernel.sym kernel.bin bochsout.txt parport.out System.map lib/libc.a kernel.SYM

distclean:
	make clean
	cd lib; make clean; cd ..;

fdimg: bzImage
	cp grub.img fd.img
	mkdir mnt
	mount fd.img mnt -oloop -tmsdos
	cp kernel mnt
	umount mnt
	rm -rf mnt
fd:
	mkdir mnt
	mount /dev/fd0 mnt
	cp kernel mnt
	umount mnt
	rm -rf mnt

run: fdimg
	bochs -f bochsrc -q
