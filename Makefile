CC = gcc
CFLAGS = -std=c11 -Wall -Werror -D_DEFAULT_SOURCE -g -O0
LDFLAGS = 

SRCDIR = $(shell basename $(shell pwd))
DESTDIR ?= 
PREFIX ?= /usr

SRC0 = daemon.c
OBJ0 = $(SRC0:%.c=%.c.o)
EXE0 = daemon

SRC1 = client.c
OBJ1 = $(SRC1:%.c=%.c.o)
EXE1 = client

all: $(EXE0) $(EXE1)

$(EXE0): $(OBJ0)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)



$(EXE1): $(OBJ1)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ0) $(EXE0) $(OBJ1) $(EXE1)

install:
	cp $(EXE0) $(EXE1) $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE0) $(DESTDIR)$(PREFIX)/bin/$(EXE1)

dist:
	cd .. && tar cvzf $(SRCDIR).tgz ./$(SRCDIR)

