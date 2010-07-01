VERSION=20100701-3
DIST=mcwm-$(VERSION)
DISTFILES=LICENSE Makefile NEWS README TODO WISHLIST config.h mcwm.c \
	list.c list.h events.h mcwm.man

CC=gcc
CFLAGS=-g -std=c99 -Wall -Wextra -I/usr/local/include #-DDEBUG #-DDMALLOC
LDFLAGS=-L/usr/local/lib -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-atom # -ldmalloc

RM=/bin/rm
PREFIX=/usr/local

TARGETS=mcwm
OBJS=mcwm.o list.o

all: $(TARGETS)

mcwm: $(OBJS) config.h events.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

mcwm-static: mcwm.c config.h events.h Makefile
	$(CC) -o $@ $(OBJS) -static -g -std=c99 -Wextra -Wall \
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
