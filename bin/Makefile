INCLUDE = -I../include
LIBS = ini.o crt0.o sys.o syscalls.o ../lib/libc.a
CC = gcc
OFLAGS = -c
CFLAGS = -g3 -Wall -Wextra -std=c99 -pedantic-errors -m32 -nostdlib -nostdinc \
	   -fno-builtin -fno-stack-protector -nostartfiles \
	   -nodefaultlibs $(INCLUDE)

# emulate ELF 32-bit, binary format being defined in in link.ld
LDFLAGS=-T link.ld -melf_i386
#LDFLAGS=-m elf_i386 -N -e start -Ttext 0x10000000
ASM = nasm
ASMFLAGS = -g -f elf

SRCS = $(wildcard *.c)
ALLOBJS = $(patsubst %.c, %.o, $(SRCS))
OBJS = $(filter-out $(LIBS), $(ALLOBJS))
PRGS = $(patsubst %.o, %, $(OBJS))

all: $(OBJS) $(LIBS) $(PRGS)
	#objdump -D -Mintel,i386 -b binary -mi386 --adjust-vma=0x10000000 cosh > cosh.lst
	#objdump -D -m i386 --adjust-vma=0x10000000 init.o > init.lst

%.o: %.c Makefile *.h
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

%:: %.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

../lib/libc.a: ../lib/Makefile
	make -C ../lib

clean:
	rm -f $(PRGS) $(OBJS) $(LIBS) *.lst
