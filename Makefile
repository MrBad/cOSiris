export CFLAGS="-j 4"
CC = gcc
DFLAGS= -DKERNEL
INCLUDE= -Iinclude
CFLAGS = -g3 -m32 -Wall -Wextra -std=c99 -nostdlib -fno-builtin\
		 -ffreestanding -c $(INCLUDE) $(DFLAGS)
ASM = nasm
ASMFLAGS = -g3 -f elf
RM = rm -f
BOCHS = bochs
QEMU = qemu-system-i386
QEMU_PARAMS = -kernel kernel.bin \
			  -drive file=hdd.img,index=0,media=disk,format=raw \
			  -serial stdio
BOOTFLAGS = -f bin
LD = ld
LDFLAGS	= -g -n -melf_i386 -T ldscript.ld -Map System.map

OBJS = i386.o port.o console.o kernel.o kinfo.o startup.o \
	   gdt.o idt.o isr.o irq.o \
	   timer.o kbd.o serial.o delay.o mem.o kheap.o vfs.o \
	   task.o sched.o syscall.o sys.o pipe.o list.o \
	   sysfile.o canonize.o bname.o hd.o hd_queue.o cofs.o \
	   rtc.o \
	   lib/libc.a

all: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)
	objdump --source kernel.bin > kernel.lst
	nm kernel.bin | sort > kernel.sym
	make -C util
	make -C bin

lib/libc.a: lib/Makefile
	make -C lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

util/cofs/mkfs:
	cd util/cofs && make && cd ../../
clean:
	$(RM) $(OBJS) fd.img kernel kernel.lst kernel.sym kernel.bin bochsout.txt\
		parport.out System.map debugger.out serial.out *.gch initrd.img core

distclean:
	make clean
	make -C lib clean
	make -C util clean || true
	make -C bin clean || true
	cd util/cofs && make clean || true && cd ../../
	rm initrd.img include/*.gch || true
	rm hdd.img

bzImage: all
	objdump --source kernel.bin > kernel.lst
	#objcopy --only-keep-debug kernel.bin kernel.sym
	nm kernel.bin | sort > kernel.sym
	#strip -s kernel.bin
	gzip -c -9 kernel.bin > kernel

fdimg: bzImage
	cp grub.img fd.img
	if [ ! -d mnt ]; then mkdir mnt; fi
	sudo mount fd.img mnt -oloop -tmsdos
	sudo cp kernel mnt
	# util/mkinitrd kernel.sym bin/init bin/test_fork bin/test_sbrk \
		# bin/test_malloc bin/cosh README.txt
	./util/mkcofs hdd.img bin/init bin/cosh bin/test_malloc bin/test_sbrk \
		bin/test_fork bin/cat bin/mkdir README.txt \
		bin/ls bin/test_write kernel.lst bin/truncate bin/test_append \
		bin/rm bin/tdup bin/pwd bin/tpipe bin/ps bin/cdc bin/reset bin/echo \
		bin/cp bin/mv bin/ln
	sudo umount mnt
	sudo rm -rf mnt

fd:
	mkdir mnt
	mount /dev/fd0 mnt
	cp kernel mnt
	umount mnt
	rm -rf mnt

diskimg: all util/cofs/mkfs
	cp kernel.bin kernel
	objdump --source kernel.bin > kernel.lst
	nm kernel.bin | sort > kernel.sym
	./util/mkdisk.sh

runb: diskimg
	$(BOCHS) -f bochsrc -q

rung: diskimg
	$(QEMU) $(QEMU_PARAMS)

debug: diskimg
	$(QEMU) $(QEMU_PARAMS) -display none -s -S

run: diskimg
	$(QEMU) $(QEMU_PARAMS) -display none

