VERSION=201000624-3
DIST=mcwm-$(VERSION)
DISTFILES=LICENSE Makefile NEWS README TODO WISHLIST config.h mcwm.c \
	events.h

CC=gcc
CFLAGS=-g -std=c99 -Wall -I/usr/local/include -L/usr/local/lib -lxcb \
	-lxcb-keysyms -lxcb-icccm -lxcb-atom #-DDEBUG

RM=/bin/rm
PREFIX=/usr/local

TARGETS=mcwm

all: $(TARGETS)

mcwm: mcwm.c config.h events.h Makefile

mcwm-static: mcwm.c config.h
	$(CC) -o mcwm-static mcwm.c -static -g -std=c99 -Wall \
	-I/usr/local/include/ -L/usr/local/lib \
	-lxcb -lxcb-keysyms -lxcb-icccm -lxcb-atom  -lxcb-property \
	-lxcb-event -lXau -lXdmcp

$(DIST).tar.bz2:
	mkdir $(DIST)
	cp $(DISTFILES) $(DIST)/
	tar cf $(DIST).tar --exclude .git $(DIST)
	bzip2 -9 $(DIST).tar
	$(RM) -rf $(DIST)

dist: $(DIST).tar.bz2

clean:
	$(RM) -f $(TARGETS)

distclean: clean
	$(RM) -f $(DIST).tar.bz2
