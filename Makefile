# Makefile for the supdup server and user.

PREFIX ?= /usr/local

CFLAGS = -g -Wall
LDFLAGS = -g
OBJS = supdup.o charmap.o
LIBS = -lncurses
EXEC = supdup

$(EXEC): $(OBJS)
	cc $(LDFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

install: supdup
	install -m 0755 supdup ${PREFIX}/bin

clean:
	rm -f $(EXEC) $(OBJS)
