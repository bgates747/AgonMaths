# Top-level Makefile for Ryu (with half-precision shared lib)
CC       ?= gcc
CXX      ?= g++
AR       ?= ar
CFLAGS   ?= -O2 -std=c99 -Wall -Wextra -fPIC
LDFLAGS  ?=
INCLUDES  = -I..

# sources
SRC       = d2fixed.c d2s.c f2s.c f16_to_s.c generic_128.c
OBJ       = $(SRC:.c=.o)

# outputs
STATIC    = libryu.a
SHARED    = libhalfryu.so

.PHONY: all clean install uninstall

all: $(STATIC) $(SHARED)

# build objects
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# static archive
$(STATIC): $(OBJ)
	$(AR) rcs $@ $^

# shared library
$(SHARED): $(OBJ)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(SHARED) -o $@ $^

install: all
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include
	install $(STATIC) $(DESTDIR)$(PREFIX)/lib/
	install $(SHARED) $(DESTDIR)$(PREFIX)/lib/
	install ryu.h   $(DESTDIR)$(PREFIX)/include/

uninstall:
	-rm -f $(DESTDIR)$(PREFIX)/lib/$(STATIC)
	-rm -f $(DESTDIR)$(PREFIX)/lib/$(SHARED)
	-rm -f $(DESTDIR)$(PREFIX)/include/ryu.h

clean:
	-rm -f $(OBJ) $(STATIC) $(SHARED)
