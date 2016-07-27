# A simple Makefile for a simple getty, no dep. handling
EXEC      = getty
prefix   ?= /usr/local
sbindir  ?= $(prefix/sbin

CPPFLAGS  = -D_GNU_SOURCE
CFLAGS    = -W -Wall -Wextra -O2 -g

all: $(EXEC)

clean:
	rm $(EXEC)

distclean: clean
	rm *~ *.o

install:
	install -t $(DESTDIR)$(prefix) getty

uninstall:
	@echo "Uninstall unsupported."

