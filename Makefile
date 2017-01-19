export CFLAGS="-j 4"
CC = gcc
CFLAGS = -g -m32 -Wall -W -nostdlib -fno-builtin -ffreestanding -c -I include
ASM = nasm
ASMFLAGS = -g -f elf
LD = ld
RM = rm -f
BOCHS=/usr/local/bochs-term/bin/bochs

BOOTFLAGS = -f bin
LDFLAGS	= -g -melf_i386 -T ldscript.ld #-Map System.map

OBJS = x86.o console.o kernel.o startup.o multiboot.o gdt.o idt.o isr.o \
		irq.o timer.o mem.o kheap.o vfs.o initrd.o task.o kbd.o serial.o lib/libc.a

bzImage: all
	objdump --source kernel.bin > kernel.lst
	#objcopy --only-keep-debug kernel.bin kernel.sym
	nm kernel.bin | sort > kernel.sym
	strip -s kernel.bin
	gzip -c -9 kernel.bin > kernel

all: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)
	make -C util
lib/libc.a: lib/Makefile
	make -C lib/ -f Makefile

startup.o: startup.asm
	$(ASM) $(ASMFLAGS) startup.asm


clean:
	$(RM) $(OBJS) fd.img kernel kernel.lst kernel.sym kernel.bin bochsout.txt parport.out System.map debugger.out serial.out 
	make -C lib clean
	make -C util clean

distclean:
	make clean
	cd lib; make clean; cd ..;

fdimg: bzImage
	cp grub.img fd.img
	if [ ! -d mnt ]; then mkdir mnt; fi
	sudo mount fd.img mnt -oloop -tmsdos
	sudo cp kernel mnt
	sudo umount mnt
	sudo rm -rf mnt
fd:
	mkdir mnt
	mount /dev/fd0 mnt
	cp kernel mnt
	umount mnt
	rm -rf mnt

run: fdimg
	$(BOCHS) -f bochsrc -q
