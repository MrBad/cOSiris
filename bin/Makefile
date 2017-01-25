INCLUDE=../include
LIBS=
CC=gcc
OFLAGS=-c
CFLAGS=-g -Wall -Wextra -std=gnu99 -pedantic-errors -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -I$(INCLUDE)
LDFLAGS=-T link.ld -melf_i386  # emulate 32 bits ELF, the binary output is specified in the linker script
ASM = nasm
ASMFLAGS = -g -f elf

OBJS=init.o crt0.o sys.o syscalls.o ../lib/libc.a

TARGET=init

all: $(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)
	objdump -b binary -m i386 -D $(TARGET) > $(TARGET).lst

../lib/libc.a: ../lib/Makefile
	make -C ../lib

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm $(OBJS) $(TARGET) *.lst
