INCLUDE = -I../include
TARGET=libc.a
CC=gcc
CFLAGS=-g3 -Wall -Wextra -std=c99 -m32 -nostdlib \
	 -fno-builtin -ffreestanding $(INCLUDE)
OFLAGS=-c
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRCS))
AR = ar
ARFLAGS = rcs

all: $(OBJS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(OFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
