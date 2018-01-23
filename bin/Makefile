INCLUDE=../include
LIBS=../lib/libc.a
CC=gcc
OFLAGS=-c
CFLAGS=-g -Wall -Wextra -std=gnu99 -pedantic-errors -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -I$(INCLUDE)
LDFLAGS=-T link.ld -melf_i386  # emulate 32 bits ELF, the binary output is specified in the linker script
ASM = nasm
ASMFLAGS = -g -f elf

OBJS=crt0.o sys.o syscalls.o ../lib/libc.a

CFILES=`ls *.c`

all: $(OBJS) init.o test_fork.o test_sbrk.o test_malloc.o cosh.o ls.o cat.o mkdir.o test_write.o truncate.o test_append.o rm.o tdup.o pwd.o tpipe.o ps.o cdc.o reset.o echo.o cp.o mv.o ln.o test_loop.o fault.o ansi.o signal.o kill.o daemon.o
	$(LD) $(LDFLAGS) -o init init.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_fork test_fork.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_sbrk test_sbrk.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_malloc test_malloc.o $(OBJS)
	$(LD) $(LDFLAGS) -o cosh cosh.o $(OBJS)
	$(LD) $(LDFLAGS) -o ls ls.o $(OBJS)
	$(LD) $(LDFLAGS) -o cat cat.o $(OBJS)
	$(LD) $(LDFLAGS) -o mkdir mkdir.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_write test_write.o $(OBJS)
	$(LD) $(LDFLAGS) -o truncate truncate.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_append test_append.o $(OBJS)
	$(LD) $(LDFLAGS) -o rm rm.o $(OBJS)
	$(LD) $(LDFLAGS) -o tdup tdup.o $(OBJS)
	$(LD) $(LDFLAGS) -o pwd pwd.o $(OBJS)
	$(LD) $(LDFLAGS) -o tpipe tpipe.o $(OBJS)
	$(LD) $(LDFLAGS) -o ps ps.o $(OBJS)
	$(LD) $(LDFLAGS) -o cdc cdc.o $(OBJS)
	$(LD) $(LDFLAGS) -o reset reset.o $(OBJS)
	$(LD) $(LDFLAGS) -o echo echo.o $(OBJS)
	$(LD) $(LDFLAGS) -o cp cp.o $(OBJS)
	$(LD) $(LDFLAGS) -o mv mv.o $(OBJS)
	$(LD) $(LDFLAGS) -o ln ln.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_loop test_loop.o $(OBJS)
	$(LD) $(LDFLAGS) -o fault fault.o $(OBJS)
	$(LD) $(LDFLAGS) -o ansi ansi.o $(OBJS)
	$(LD) $(LDFLAGS) -o signal signal.o $(OBJS)
	$(LD) $(LDFLAGS) -o kill kill.o $(OBJS)
	$(LD) $(LDFLAGS) -o daemon daemon.o $(OBJS)
	#objdump -D -Mintel,i386 -b binary -m i386 --adjust-vma=0x10000000 cosh > cosh.lst
../lib/libc.a: ../lib/Makefile
	make -C ../lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm *.o init test_fork test_sbrk test_malloc cosh ls cat mkdir test_write truncate test_append rm tdup pwd tpipe ps cdc echo reset cp ln mv test_loop ansi fault signal kill daemon
