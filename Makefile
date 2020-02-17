# Makefile for the supdup server and user.

PREFIX ?= /usr/local

CFLAGS = -g -Wall
LDFLAGS = -g
OBJS = supdup.o charmap.o
LIBS = -lncurses
CLIENT = supdup
SERVER = supdupd
CC = cc

# The server isn't ready for prime time.
all:	$(CLIENT)

$(CLIENT): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SERVER): supdupd.o
	$(CC) $(LDFLAGS) -o $@ $^

install: $(CLIENT) $(SERVER)
	install -m 0755 supdup ${PREFIX}/bin
	test -x supdupd && install -m 0755 supdupd ${PREFIX}/bin

clean:
	rm -f $(CLIENT) $(SERVER) $(OBJS)
