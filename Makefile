CC = gcc
CFLAGS = -g -Wall -W -nostdlib -fno-builtin -ffreestanding -c -I include
ASM = nasm
ASMFLAGS = -g -f elf
LD = ld
RM = rm -f

BOOTFLAGS = -f bin
LDFLAGS	= -g -T lscript.txt -Map System.map

OBJS = x86.o console.o kernel.o startup.o multiboot.o gdt.o idt.o isr.o \
		irq.o timer.o mem.o kheap.o libc.a

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

kernel.o: kernel.c kernel.h
	$(CC) $(CFLAGS) -o kernel.o kernel.c

console.o: console.c console.h
	$(CC) $(CFLAGS) -o console.o console.c

multiboot.o: multiboot.c multiboot.h
	$(CC) $(CFLAGS) -o multiboot.o multiboot.c

gdt.o: gdt.c gdt.h
	$(CC) $(CFLAGS) -o gdt.o gdt.c

idt.o: idt.c idt.h
	$(CC) $(CFLAGS) -o idt.o idt.c

isr.o: isr.c isr.h
	$(CC) $(CFLAGS) -o isr.o isr.c

irq.o: irq.c irq.h
	$(CC) $(CFLAGS) -o irq.o irq.c

x86.o: x86.c x86.h
	$(CC) $(CFLAGS) -o x86.o x86.c

timer.o: timer.c timer.h
	$(CC) $(CFLAGS) -o timer.o timer.c

mem.o: mem.c mem.h
	$(CC) $(CFLAGS) -o mem.o mem.c

kheap.o: kheap.c kheap.h
	$(CC) $(CFLAGS) -o kheap.o kheap.c

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
