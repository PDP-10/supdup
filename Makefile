# Makefile for the supdup server and user.

# Definitions for supdup.
# TERMCAP	Uses the termcap database. On many systems this might
#		require installing ncurses first 
#		(e.g. in Ubuntu - libncurses-dev).
# TERMINFO	Uses the terminfo database.  Exactly one of TERMCAP or
#		TERMINFO must be defined and the corresponding library
#		(-ltermcap or -lterminfo) must be linked in.
# DEBUG
PREFIX ?= /usr/local

supdup: supdup.c termcaps.h
	cc -g -o supdup -DTERMCAP supdup.c -ltermcap

install: supdup
	install -m 0755 supdup ${PREFIX}/bin

clean:
	rm -rf supdup
