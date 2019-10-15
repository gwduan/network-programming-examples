CC=gcc
CFLAGS=-g -O -Wall -W -pedantic

TARGETS=tcp4server tcp4client udp4server udp4client unixdgramclient unixdgramserver

all:$(TARGETS)

tcp4server: tcp4server.o common.o data.o
tcp4client: tcp4client.o common.o data.o
udp4server: udp4server.o
udp4client: udp4client.o
unixdgramclient: unixdgramclient.o
unixdgramserver: unixdgramserver.o

.PHONY: clean

clean:
	rm -rf $(TARGETS) *.o
