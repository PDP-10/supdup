# Makefile for the supdup server and user.

PREFIX ?= /usr/local

CFLAGS = -g -Wall
LDFLAGS = -g
OBJS = supdup.o
LIBS = -lncurses
EXEC = supdup
CC = cc

all:	$(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

install: supdup
	install -m 0755 supdup ${PREFIX}/bin

clean:
	rm -f $(EXEC) $(OBJS)
