INCLUDE=../include
LIBS=../lib/libc.a
CC=gcc
OFLAGS=-c
CFLAGS=-g -Wall -Wextra -std=gnu99 -pedantic-errors -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -I$(INCLUDE)
LDFLAGS=-T link.ld -melf_i386  # emulate 32 bits ELF, the binary output is specified in the linker script
ASM = nasm
ASMFLAGS = -g -f elf

OBJS=crt0.o sys.o syscalls.o ../lib/libc.a


all: $(OBJS) init.o test_fork.o test_sbrk.o
	$(LD) $(LDFLAGS) -o init init.o $(OBJS)
	objdump -b binary -m i386 -D init > init.lst
	$(LD) $(LDFLAGS) -o test_fork test_fork.o $(OBJS)
	$(LD) $(LDFLAGS) -o test_sbrk test_sbrk.o $(OBJS)
	# objdump -b binary -m i386 -D $(TARGET) > $(TARGET).lst

../lib/libc.a: ../lib/Makefile
	make -C ../lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm *.o init test_fork test_sbrk *.lst *.gch
