CC=gcc
CFLAGS=-g -O -Wall -W -pedantic
#CFLAGS=-g -O -Wall -W -pedantic -DDEBUG

TARGETS=tcp4server tcp4client udp4server udp4client unixdgramclient unixdgramserver unixstreamserver unixstreamclient tcp4server-nonblock-accept tcp4client-nonblock-connect tcp4server-poll tcp4server-epoll tcp4server-select unixstreamserver-tranfd unixstreamclient-tranfd

all:$(TARGETS)

tcp4server: tcp4server.o common.o data.o
tcp4client: tcp4client.o common.o data.o
udp4server: udp4server.o
udp4client: udp4client.o
unixstreamserver: unixstreamserver.o common.o data.o
unixstreamclient: unixstreamclient.o common.o data.o
unixdgramclient: unixdgramclient.o
unixdgramserver: unixdgramserver.o
tcp4server-nonblock-accept: tcp4server-nonblock-accept.o common.o data-nonblock.o
tcp4client-nonblock-connect: tcp4client-nonblock-connect.o common.o data-nonblock.o
tcp4server-poll: tcp4server-poll.o common.o
tcp4server-epoll: tcp4server-epoll.o common.o
tcp4server-select: tcp4server-select.o common.o

unixstreamserver-tranfd: unixstreamserver.o common.o data-tranfd.o
	$(CC) -o $@ $^
unixstreamclient-tranfd: unixstreamclient.o common.o data-tranfd.o
	$(CC) -o $@ $^

.PHONY: clean

clean:
	rm -rf $(TARGETS) *.o
