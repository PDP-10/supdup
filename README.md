# `supdup`

## Overview

* `supdup` is a client (and `supdupd` is a server) implementing the SUPDUP (*Sup*er *Dup*er TELNET) protocol, a highly efficient [TELNET](https://www.rfc-editor.org/rfc/rfc854.txt)-style remote display protocol, on UNIX-like systems.

* It originated as a private protocol between the [Chaosnet](https://chaosnet.net/)-connected ITS systems at MIT to allow a user at any one of these systems to use one of the others as a display; later implementations were developed for [various platforms](https://gunkies.org/wiki/Chaosnet#Protocol_implementations).

* The software creates connections over TCP/IP and Chaosnet (optionally using the [cbridge](https://github.com/bictorv/chaosnet-bridge) software).

## Tested platforms

* Tested operating systems: Oracle Solaris, OpenIndiana illumos, Linux/musl, Linux/glibc, IBM AIX, IBM OS/400 (PASE), Haiku, FreeBSD, OpenBSD, NetBSD, DragonFly BSD, Cygwin, and Apple macOS.

## Tested compilers

* Tested compilers: PCC, GCC, Clang, Xcode, IBM XL C, IBM Open XL C, Oracle Studio C, NVIDIA HPC SDK C, Portland Group C, and DMD ImportC.

## Building the `supdup` client

### Generic UNIX

* Building on generic UNIX-like systems (**Linux**, **\*BSD**, **macOS**, **Haiku**, etc.):
  * Install prerequisite packages:
    * Required: `make` (GNU), `ncurses` (libraries and headers),
    * Recommended: `pkg-config`
  * Build: `gmake` (or sometimes just `make`)
[]()

[]()
* The usual environment variables (*e.g.* `CC`, `CFLAGS`, `LDFLAGS`, etc.) are respected, for example:
  * `env CC="gcc" CFLAGS="-Wall" LDFLAGS="-L/usr/local" gmake`

### IBM AIX

* Building on IBM AIX 7:
  * Install prerequisite packages:
    * `dnf install make ncurses-devel pkg-config`
  * Build with IBM XL C V16:
    * `env CC="xlc" CFLAGS="-q64" LDFLAGS="-q64" gmake`
  * Build with IBM Open XL C V17:
    * `env CC="ibm-clang" CFLAGS="-m64" LDFLAGS="-m64" gmake`
  * Build with GNU GCC:
    * `env CC="gcc" CFLAGS="-maix64" LDFLAGS="-maix64 -Wl,-b64" gmake`
  * Build with Clang:
    * `env CC="clang" CFLAGS="-m64" LDFLAGS="-m64" gmake`

### IBM i (OS/400)

* Building on PASE for IBM i (OS/400):
  * Install prerequisite packages:
    * `yum install gcc10 make-gnu ncurses-devel pkg-config`
  * Build with GNU GCC:
    * `env CC="gcc-10" CFLAGS="-maix64" LDFLAGS="-maix64 -Wl,-b64" gmake`

### DMD ImportC

* Building with DMD ImportC:
  * `dmd -betterC -c -of=supdup.o chaos.c charmap.c supdup.c tcp.c $(pkg-config --cflags-only-I ncurses)`
  * `cc -o supdup supdup.o $(pkg-config --libs ncurses)`

## Building the `supdup` server

* An *experimental* server, `supdupd`, is included.
* The server component is *antiquated*, *incomplete*, and *will not* build on all platforms.
[]()

[]()
* To build the server on generic UNIX-like systems:
  * Install prerequisite packages: `make` (GNU)
  * Build: `gmake` (or sometimes just `make`)

## Bug Reporting

* To report a problem with `supdup`, use GitHub Issues:
  * https://github.com/PDP-10/supdup/issues/new/choose

## External links

### SUPDUP

* [RFC 734: SUPDUP Protocol](https://www.rfc-editor.org/rfc/rfc734.txt)
* [RFC 736: TELNET SUPDUP Option](https://www.rfc-editor.org/rfc/rfc736.txt)
* [RFC 746: The SUPDUP Graphics Extension](https://www.rfc-editor.org/rfc/rfc746.txt)
* [RFC 747: Recent Extensions to the SUPDUP Protocol](https://www.rfc-editor.org/rfc/rfc747.txt)
* [RFC 749: TELNET SUPDUP-OUTPUT Option](https://www.rfc-editor.org/rfc/rfc749.txt)
* [AI Memo 643: A Local Front End for Remote Editing](http://www.bitsavers.org/pdf/mit/ai/aim/AIM-643.pdf)
* [AI Memo 644: The SUPDUP Protocol](http://www.bitsavers.org/pdf/mit/ai/aim/AIM-644.pdf)
* [PuTTY: SUPDUP Backend](https://git.tartarus.org/?p=simon/putty.git;a=blob;f=otherbackends/supdup.c;h=6f574c9fb9c34b1307b67326038aa713c2b1d07a;hb=HEAD)

### Chaosnet

* [AI Memo 628: Chaosnet](http://bitsavers.org/pdf/mit/ai/AIM-628_chaosnet.pdf)
* [Chaosnet Bridge](https://github.com/bictorv/chaosnet-bridge)
* [Chaosnet Wiki](https://chaosnet.net/)
