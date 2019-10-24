#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include "common.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define TIME_OUT 1

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	int ret;
	int connected = 0;
	fd_set readfds, writefds;
	struct timeval tv;
	int err;
	socklen_t errlen;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (set_nonblocking(sockfd) == -1) {
		perror("set_nonblocking");
		close(sockfd);
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if (!(ret = inet_pton(AF_INET, SERVER_IP, &(servaddr.sin_addr)))) {
		fprintf(stderr, "bad address: [%s]!\n", SERVER_IP);
		close(sockfd);
		exit(1);
	} else if (ret == -1) {
		perror("inet_pton");
		close(sockfd);
		exit(1);
	}
	servaddr.sin_port = htons(SERVER_PORT);

	if (!connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) {
		/* success immediately */
		fprintf(stdout, "connected immediately.\n");
		connected = 1;
	} else if (errno != EINPROGRESS) {
		perror("connect");
		close(sockfd);
		exit(1);
	}

	/* do something else */

	/* wait connection to establish */
	if (!connected) {
		fprintf(stdout, "wait connection to establish.\n");
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(sockfd, &readfds);
		FD_SET(sockfd, &writefds);
		tv.tv_sec = TIME_OUT;
		tv.tv_usec = 0;
		switch(select(sockfd + 1, &readfds, &writefds, NULL, &tv)) {
		case -1:
			perror("select");
			close(sockfd);
			exit(1);
		case 0:
			fprintf(stderr, "connect timeout.\n");
			close(sockfd);
			exit(1);
		}

		errlen = sizeof(err);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err,
					&errlen) == -1) {
			perror("getsockopt");
			close(sockfd);
			exit(1);
		}
		if ((errno = err)) {
			perror("connect");
			close(sockfd);
			exit(1);
		}
	}

	do_client(sockfd);
	close(sockfd);

	exit(0);
}
