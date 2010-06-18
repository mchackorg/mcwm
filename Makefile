VERSION=201000618
DIST=mcwm-$(VERSION)
DISTFILES=LICENSE Makefile README TODO WISHLIST config.h mcwm.c

CC=gcc
CFLAGS=-g -std=c99 -Wall -I/usr/local/include -L/usr/local/lib -lxcb \
	-lxcb-keysyms

# Define -DDEBUG for lots of debug information.

#CFLAGS=-g -std=c99 -Wall -I/usr/local/include -L/usr/local/lib -lxcb -lxcb-keysyms \
#	-DDMALLOC -DMALLOC_FUNC_CHECK -ldmalloc


RM=/bin/rm

TARGETS=mcwm

all: $(TARGETS)

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
