export CFLAGS="-j 4"
CC = gcc
DFLAGS=
INCLUDE=include
CFLAGS = -g3 -m32 -Wall -Wextra -std=gnu90 -nostdlib -fno-builtin\
		 -ffreestanding -c -I$(INCLUDE) $(DFLAGS)
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

OBJS = x86.o console.o kernel.o kinfo.o startup.o gdt.o idt.o isr.o irq.o \
	   timer.o kbd.o serial.o delay.o mem.o kheap.o vfs.o \
	   task.o sched.o syscall.o sys.o pipe.o list.o \
	   sysfile.o canonize.o bname.o hd.o hd_queue.o cofs.o \
	   rtc.o \
	   lib/libc.a

all: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)
	make -C util
	make -C bin

lib/libc.a: lib/Makefile
	make -C lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	$(RM) $(OBJS) fd.img kernel kernel.lst kernel.sym kernel.bin bochsout.txt\
		parport.out System.map debugger.out serial.out *.gch initrd.img

distclean:
	make clean
	make -C lib clean
	make -C util clean
	make -C bin clean
	rm initrd.img include/*.gch

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
		# bin/test_malloc bin/cosh README
	./util/mkcofs hdd.img bin/init bin/cosh bin/test_malloc bin/test_sbrk \
		bin/test_fork bin/cat bin/mkdir README \
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

diskimg: all
	cp kernel.bin kernel
	objdump --source kernel.bin > kernel.lst
	nm kernel.bin | sort > kernel.sym
	./util/mkdisk.sh
	./util/mkcofs hdd.img bin/init bin/cosh bin/test_malloc bin/test_sbrk \
		bin/test_fork bin/cat bin/mkdir README \
		bin/ls bin/test_write kernel.lst bin/truncate bin/test_append \
		bin/rm bin/tdup bin/pwd bin/tpipe bin/ps bin/cdc bin/reset bin/echo \
		bin/cp bin/mv bin/ln

runb: diskimg
	$(BOCHS) -f bochsrc -q

rung: all
	$(QEMU) $(QEMU_PARAMS)

debug: all
	$(QEMU) $(QEMU_PARAMS) -display none -s -S

run: diskimg
	$(QEMU) $(QEMU_PARAMS) -display none

