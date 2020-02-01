.POSIX:

CC     ?= tcc
PREFIX ?= /usr/local

X11LIB = /usr/X11R6/lib
LIBS   = -L${X11LIB} -lX11
CFLAGS = -std=c99 -pedantic -Wall -O3 ${LIBS}

all: clayout

clean:
	rm -f clayout

clayout:
	$(CC) -o $@ ${CFLAGS} layout.c

install: clayout
	cp -f clayout $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/clayout

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/clayout

.PHONY: all clean clayout install uninstall
