VERSION=201000629
DIST=mcwm-$(VERSION)
DISTFILES=LICENSE Makefile NEWS README TODO WISHLIST config.h mcwm.c \
	events.h mcwm.man

CC=gcc
CFLAGS=-g -std=c99 -Wall -I/usr/local/include #-DDEBUG
LDFLAGS=-L/usr/local/lib -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-atom

RM=/bin/rm
PREFIX=/usr/local

TARGETS=mcwm
OBJS=mcwm.o list.o

all: $(TARGETS)

mcwm: $(OBJS) config.h events.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

mcwm-static: mcwm.c config.h
	$(CC) -o mcwm-static mcwm.c -static -g -std=c99 -Wall \
	-I/usr/local/include/ -L/usr/local/lib \
	-lxcb -lxcb-keysyms -lxcb-icccm -lxcb-atom  -lxcb-property \
	-lxcb-event -lXau -lXdmcp

install: $(TARGETS)
	install -m 755 mcwm $(PREFIX)/bin

deinstall:
	$(RM) $(PREFIX)/bin/mcwm

$(DIST).tar.bz2:
	mkdir $(DIST)
	cp $(DISTFILES) $(DIST)/
	tar cf $(DIST).tar --exclude .git $(DIST)
	bzip2 -9 $(DIST).tar
	$(RM) -rf $(DIST)

dist: $(DIST).tar.bz2

clean:
	$(RM) -f $(TARGETS) *.o

distclean: clean
	$(RM) -f $(DIST).tar.bz2
