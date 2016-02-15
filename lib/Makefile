.SUFFIXES: .asm

# defines
MAKEFILE=Makefile
MAKEDEP=$(MAKEFILE)
INCDIR	=../include
NASM	=nasm -f elf -i$(INCDIR)/
#CC	=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)
CC	=gcc -m32 -g -Wall -W -O2 -nostdlib -fno-builtin -ffreestanding -I$(INCDIR)
#STDIO	=stdio/doprintf.o

STRING	=memcpy.o strcpy.o strlen.o strcmp.o memset.o
STDARG	=vsprintf.o
#X86	=x86/inportb.o x86/outportb.o x86/disable.o x86/enable.o

#CTYPE	=ctype/ctype.o

#SETJMP	=setjmp/setjmp.o setjmp/longjmp.o

OBJS	=$(STRING) $(STDARG)

# targets
all: libc.a

clean:
	rm -f libc.a $(OBJS)

# implicit rules
#.asm.o:
#	$(NASM) -o$@ $<

.c.o:
	$(CC) -c -o$@ $<

# dependencies
memcpy.o:	memcpy.c		$(MAKEDEP)
strcpy.o:	strcpy.c		$(MAKEDEP)
strcmp.o:	strcmp.c		$(MAKEDEP)
vsprintf.o:	vsprintf.c		$(MAKEDEP)
memset.o:	memset.c		$(MAKEDEP)

# explicit rules
libc.a:	$(OBJS) $(MAKEDEP)
	ar rcs libc.a $(OBJS)
