CC=gcc
CFLAGS=-g -O -Wall -W -pedantic

TARGETS=tcp4server tcp4client

all:$(TARGETS)

tcp4server: tcp4server.o common.o data.o
tcp4client: tcp4client.o common.o data.o

.PHONY: clean

clean:
	rm -rf $(TARGETS) *.o
