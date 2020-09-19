# Makefile for the supdup server and user.

PREFIX ?= /usr/local

CFLAGS = -g -Wall
LDFLAGS = -g
OBJS = supdup.o charmap.o
LIBS = -lncurses
CC = cc

# The server isn't ready for prime time.
all:	supdup

supdup: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

supdupd: supdupd.o
	$(CC) $(LDFLAGS) -o $@ $^

install: supdup supdupd
	install -m 0755 supdup ${PREFIX}/bin
	test -x supdupd && install -m 0755 supdupd ${PREFIX}/bin

clean:
	rm -f *.o
	rm -f supdup supdupd
