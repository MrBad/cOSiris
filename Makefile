# Comment it out if you do not whant kdbg support
#CONFIG_KDBG=y

export CFLAGS="-j 4"
CC = gcc
DFLAGS= -DKERNEL
INCLUDE= -Iinclude
CFLAGS = -g3 -Wall -Wextra -std=c99 -m32 -nostdlib -fno-builtin \
		 -ffreestanding $(INCLUDE) $(DFLAGS)
OFLAGS = -c
ASM = nasm
ASMFLAGS = -g3 -f elf
RM = rm -f
BOCHS = bochs
QEMU = sudo qemu-system-i386
QEMU_SKIP_GRUB= -kernel kernel.bin
QEMU_PARAMS = \
			  -drive file=hdd.img,index=0,media=disk,format=raw \
			  -serial stdio
BOOTFLAGS = -f bin
LD = ld
LDFLAGS	= -g -n -melf_i386 -T ldscript.ld -Map System.map

OBJS = startup.o kernel.o i386.o port.o kinfo.o ring_buf.o \
	   gdt.o idt.o int.o irq.o delay.o timer.o rtc.o \
	   dev.o ansi.o kbd.o crt.o serial.o console.o tty.o \
	   mem.o kheap.o \
	   task.o fork.o thread.o sys.o syscall.o signal.o sysproc.o \
	   list.o bname.o canonize.o \
	   hd.o hd_queue.o cofs.o vfs.o pipe.o sysfile.o \
	   pci.o driver.o ne2000.o \
	   net.o arp.o ip.o icmp.o sock.o udp.o bmap.o dhcp.o route.o dns.o tcp.o

ifdef CONFIG_KDBG
OBJS += kdbg.o kdbg0.o
DFLAGS += -DKDBG
endif

OBJS += lib/libc.a

all: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)
	objdump --source kernel.bin > kernel.lst
	nm kernel.bin | sort > kernel.sym
	make -C util
	make -C bin

lib/libc.a: lib/Makefile
	make -C lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

util/cofs/mkfs:
	cd util/cofs && make && cd ../../
clean:
	$(RM) $(OBJS) fd.img kernel kernel.lst kernel.sym kernel.bin bochsout.txt\
		parport.out System.map debugger.out serial.out *.gch initrd.img core \
		debug.log tags

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
	ctags -R .
	cp kernel.bin kernel
	objdump --source kernel.bin > kernel.lst
	nm kernel.bin | sort > kernel.sym
	./util/mkdisk.sh

runb: diskimg
	$(BOCHS) -f bochsrc -q

rung: diskimg
	$(QEMU) $(QEMU_SKIP_GRUB) $(QEMU_PARAMS)

debug: diskimg
	$(QEMU) $(QEMU_SKIP_GRUB) $(QEMU_PARAMS) -display none -s -S \
		-netdev tap,helper=/usr/lib/qemu/qemu-bridge-helper,id=cOSnet\
		-device ne2k_pci,netdev=cOSnet
	#-netdev user,id=cOSnet -device ne2k_pci,netdev=cOSnet,mac=11:22:33:44:55:66

run: diskimg
	#sudo ip link add br0 type bridge
	$(QEMU) $(QEMU_SKIP_GRUB) $(QEMU_PARAMS) -display none \
		-serial telnet:127.0.0.1:1234,server,nowait \
		-netdev tap,helper=/usr/lib/qemu/qemu-bridge-helper,id=cOSnet\
		-device ne2k_pci,netdev=cOSnet
		#-net nic,model=ne2k_pci -net tap,script=no,ifname=tap1
		#-net nic,model=ne2k_pci -net dump,file=net.pcap -net user

