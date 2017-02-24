INCLUDE=../include
LIBS=../lib/libc.a
CC=gcc
OFLAGS=-c
CFLAGS=-g -Wall -Wextra -std=gnu99 -pedantic-errors -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -I$(INCLUDE)
LDFLAGS=-T link.ld -melf_i386  # emulate 32 bits ELF, the binary output is specified in the linker script
ASM = nasm
ASMFLAGS = -g -f elf

OBJS=crt0.o sys.o syscalls.o ../lib/libc.a


all: $(OBJS) init.o test_fork.o test_sbrk.o test_malloc.o cosh.o ls.o cat.o mkdir.o test_write.o truncate.o test_append.o rm.o tdup.o pwd.o tpipe.o ps.o cdc.o
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
	

	# objdump -b binary -m i386 -D $(TARGET) > $(TARGET).lst

../lib/libc.a: ../lib/Makefile
	make -C ../lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm *.o init test_fork test_sbrk test_malloc cosh ls cat mkdir test_write truncate test_append rm tdup pwd tpipe ps cdc
