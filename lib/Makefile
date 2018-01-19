.SUFFIXES: .asm

# defines
MAKEFILE=Makefile
MAKEDEP=$(MAKEFILE)
INCDIR	=../include
NASM	=nasm -f elf -i$(INCDIR)/
#CC	=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)
CC	=gcc -m32 -g -Wall -W -O2 -nostdlib -fno-builtin -ffreestanding -I$(INCDIR)

STRING	=memcpy.o memmove.o strcpy.o strncpy.o strlen.o strcmp.o memset.o strdup.o strcat.o strncat.o strchr.o
STDARG	=vsprintf.o sprintf.o
STDIO	=printf.o
STDLIB 	=malloc.o
#X86	=x86/inportb.o x86/outportb.o x86/disable.o x86/enable.o
#CTYPE	=ctype/ctype.o
#SETJMP	=setjmp/setjmp.o setjmp/longjmp.o

OBJS	=$(STRING) $(STDARG) $(STDIO) $(STDLIB)

# targets
all: libc.a

clean:
	rm -f libc.a $(OBJS) *.gch

# implicit rules
#.asm.o:
#	$(NASM) -o$@ $<

.c.o:
	$(CC) -c -o $@ $<

# dependencies
memcpy.o:	memcpy.c		$(MAKEDEP)
strcpy.o:	strcpy.c		$(MAKEDEP)
strncpy.o:	strncpy.c		$(MAKEDEP)
strcmp.o:	strcmp.c		$(MAKEDEP)
strchr.o:	strchr.c		$(MAKEDEP)
vsprintf.o:	vsprintf.c		$(MAKEDEP)
sprintf.o:  sprintf.c vsprintf.c $(MAKEDEP)
memset.o:	memset.c		$(MAKEDEP)
strdup.o:	strdup.c		$(MAKEDEP)
strcat.o:	strcat.c		$(MAKEDEP)
strncat.o:	strncat.c		$(MAKEDEP)
malloc.o:	malloc.c		$(MAKEDEP)
# explicit rules
libc.a:	$(OBJS) $(MAKEDEP)
	ar rcs libc.a $(OBJS)
