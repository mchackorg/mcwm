VERSION=20101118-2
DIST=mcwm-$(VERSION)
SRC=mcwm.c list.c config.h events.h list.h
DISTFILES=LICENSE Makefile NEWS README TODO WISHLIST mcwm.man $(SRC)

CC=gcc
CFLAGS=-g -std=c99 -Wall -Wextra -O2 -I/usr/local/include #-DDEBUG #-DDMALLOC
LDFLAGS=-L/usr/local/lib -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-atom # -ldmalloc

RM=/bin/rm
PREFIX=/usr/local

TARGETS=mcwm
OBJS=mcwm.o list.o

all: $(TARGETS)

mcwm: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

mcwm-static: $(OBJS)
	$(CC) -o $@ $(OBJS) -static $(CFLAGS) $(LDFLAGS) \
	-lxcb-property -lxcb-event -lXau -lXdmcp

mcwm.o: mcwm.c events.h list.h config.h Makefile

list.o: list.c list.h Makefile

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
