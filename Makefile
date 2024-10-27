# Makefile for the supdup server and client.

SHELL=/bin/sh
PREFIX?=/usr/local

OS_NAME?=$(shell uname 2> /dev/null)
CFLAGS+=$(shell pkg-config --cflags ncurses 2> /dev/null)
NCURSES_LIBS?=$(shell pkg-config --libs ncurses 2> /dev/null || printf '%s' "-lncurses")

# AIX (requires linking libcurses *after* libncurses!)
ifeq ($(OS_NAME),AIX)
 EXTRA_LDFLAGS=-lcurses
endif

# Mac OS X
ifeq ($(OS_NAME),Darwin)
 ifneq (,$(wildcard /opt/local/include))
  CFLAGS+=-I/opt/local/include
 else
  ifneq (,$(wildcard /usr/local/include))
   CFLAGS+=-I/usr/local/include
  endif
 endif
 ifneq (,$(wildcard /opt/local/lib))
  LDFLAGS+=-L/opt/local/lib
 else
  ifneq (,$(wildcard /usr/local/lib))
   LDFLAGS+=-L/usr/local/lib
  endif
 endif
endif

# Solaris and illumos
ifeq ($(OS_NAME),SunOS)
 CFLAGS+=-I/usr/include/ncurses
 EXTRA_LDFLAGS=-lnsl -lsocket
endif

# Haiku
ifeq ($(OS_NAME),Haiku)
 EXTRA_LDFLAGS=-lnetwork
endif

# NetBSD
ifeq ($(OS_NAME),NetBSD)
 CFLAGS+=-I/usr/pkg/include -I/usr/pkg/include/ncurses
 LDFLAGS+=-L/usr/pkg/lib
endif

# The server (supdupd) and supdup-login aren't ready for prime time.
.PHONY: all
all: supdup

SUPDUP_OBJS = supdup.o charmap.o tcp.o chaos.o
supdup: $(SUPDUP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SUPDUP_OBJS) $(NCURSES_LIBS) $(EXTRA_LDFLAGS)

SUPDUPD_OBJS = supdupd.o
supdupd: $(SUPDUPD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SUPDUPD_OBJS)

.PHONY: install
install: supdup
	install -m 0755 supdup $(PREFIX)/bin
	test -x supdupd && install -m 0755 supdupd $(PREFIX)/bin

.PHONY: clean
clean:
	$(RM) *.o
	$(RM) supdup supdupd

.PHONY: distclean
distclean: clean
	$(RM) *~ *.bak core *.core

.PHONY: printvars printenv
printvars printenv:
	-@printf '%s: ' "FEATURES" 2> /dev/null
	-@printf '%s ' "$(.FEATURES)" 2> /dev/null
	-@printf '%s\n' "" 2> /dev/null
	-@$(foreach V,$(sort $(.VARIABLES)), \
	  $(if $(filter-out environment% default automatic,$(origin $V)), \
	  $(if $(strip $($V)),$(info $V: [$($V)]),)))
	-@true > /dev/null 2>&1

.PHONY: print-%
print-%:
	-@$(info $*: [$($*)] ($(flavor $*). set by $(origin $*)))@true
	-@true > /dev/null 2>&1

# Dependencies
chaos.o: chaos.c supdup.h
charmap.o: charmap.c charmap.h
supdup.o: supdup.c supdup.h charmap.h
supdupd.o: supdupd.c supdup.h
supdup-login.o: supdup-login.c
tcp.o: tcp.c supdup.h
