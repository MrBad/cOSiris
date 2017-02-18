export CFLAGS="-j 4"
CC = gcc
CFLAGS = -g -m32 -Wall -nostdlib -fno-builtin -ffreestanding -c -I include
ASM = nasm
ASMFLAGS = -g -f elf
LD = ld
RM = rm -f
BOCHS=/usr/local/bochs-term/bin/bochs

BOOTFLAGS = -f bin
LDFLAGS	= -g -melf_i386 -T ldscript.ld #-Map System.map

OBJS =	x86.o console.o kernel.o startup.o multiboot.o gdt.o idt.o isr.o irq.o \
		timer.o kbd.o serial.o delay.o mem.o kheap.o vfs.o \
		task.o sched.o syscall.o sys.o pipe.o list.o \
		sysfile.o canonize.o hd.o hd_queue.o cofs.o \
		lib/libc.a

bzImage: all
	objdump --source kernel.bin > kernel.lst
	#objcopy --only-keep-debug kernel.bin kernel.sym
	nm kernel.bin | sort > kernel.sym
	strip -s kernel.bin
	gzip -c -9 kernel.bin > kernel

all: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)
	make -C util
	make -C bin
	make -C test

lib/libc.a: lib/Makefile
	make -C lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	$(RM) $(OBJS) fd.img kernel kernel.lst kernel.sym kernel.bin bochsout.txt parport.out System.map debugger.out serial.out *.gch initrd.img


distclean:
	make clean
	make -C lib clean
	make -C util clean
	make -C bin clean
	make -C test clean
	rm initrd.img include/*.gch

fdimg: bzImage
	cp grub.img fd.img
	if [ ! -d mnt ]; then mkdir mnt; fi
	sudo mount fd.img mnt -oloop -tmsdos
	sudo cp kernel mnt
	# sudo cp bin/init mnt
	# util/mkinitrd kernel.sym bin/init bin/test_fork bin/test_sbrk \
		# bin/test_malloc bin/cosh README
	./util/mkcofs hdd.img bin/init bin/cosh bin/test_malloc bin/test_sbrk \
		bin/test_fork bin/cat bin/mkdir README \
		bin/ls test/dir test/p test/lss test/cd

	# sudo cp initrd.img mnt
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
runq: fdimg
	qemu-system-i386 -kernel kernel.bin -curses -initrd initrd.img -drive file=hdd.img,index=0,media=disk,format=raw
