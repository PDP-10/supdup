# Makefile for the supdup server and client.

PREFIX ?= /usr/local

CC = cc
CFLAGS = -g -Wall
LDFLAGS = -g

# The server isn't ready for prime time.
all:	supdup

SUPDUP_OBJS = supdup.o charmap.o
supdup: $(SUPDUP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SUPDUP_OBJS) -lncurses

SUPDUPD_OBJS = supdupd.o
supdupd: $(SUPDUPD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SUPDUPD_OBJS)

install: supdup
	install -m 0755 supdup $(PREFIX)/bin
	test -x supdupd && install -m 0755 supdupd $(PREFIX)/bin

clean:
	rm -f *.o
	rm -f supdup supdupd
