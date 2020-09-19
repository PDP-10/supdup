# Makefile for the supdup server and client.

PREFIX ?= /usr/local

CC = cc
CFLAGS = -g -Wall
LDFLAGS = -g

# The server isn't ready for prime time.
all:	supdup

supdup: supdup.o charmap.o
	$(CC) $(LDFLAGS) -o $@ $^ -lncurses

supdupd: supdupd.o
	$(CC) $(LDFLAGS) -o $@ $^

install: supdup
	install -m 0755 supdup $(PREFIX)/bin
	test -x supdupd && install -m 0755 supdupd $(PREFIX)/bin

clean:
	rm -f *.o
	rm -f supdup supdupd
