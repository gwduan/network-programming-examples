#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "common.h"

int do_server(int connfd)
{
	int n;
	char buf[1024000];
	int time = 0;

	for (;;) {
		if ((n = readn_nonblock(connfd, buf, sizeof(buf))) == 0)
			return 0;

		time++;
		fprintf(stdout, "%d read for %d\n", n, time);
		usleep(1000);
	}
}

#define MESSAGE_SIZE 1024000000

int do_client(int connfd)
{
	char *query;
	int i;

	query = malloc(MESSAGE_SIZE + 1);
	for (i = 0; i < MESSAGE_SIZE; i++) {
		query[i] = 'a';
	}
	query[MESSAGE_SIZE] = '\0';

	if (writen_nonblock(connfd, query, strlen(query)) == -1) {
		return -1;
	}
	fprintf(stdout, "send into buffer end.\n");

	return 0;
}
