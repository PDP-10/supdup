# Makefile for the supdup server and user.

PREFIX ?= /usr/local

OS_NAME = $(shell uname)
ifeq ($(OS_NAME), Darwin)
OS = OSX
endif


CFLAGS = -g -Wall
# Mac OSX
ifeq ($(OS), OSX)
LDFLAGS = -L/opt/local/lib
endif
OBJS = supdup.o charmap.o
LIBS = -lncurses -lresolv
CLIENT = supdup
SERVER = supdupd
CC = cc

# The server isn't ready for prime time.
all:	$(CLIENT)

$(CLIENT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SERVER): supdupd.o
	$(CC) $(LDFLAGS) -o $@ $^

install: $(CLIENT) $(SERVER)
	install -m 0755 supdup ${PREFIX}/bin
	test -x supdupd && install -m 0755 supdupd ${PREFIX}/bin

clean:
	rm -f $(CLIENT) $(SERVER) $(OBJS)
