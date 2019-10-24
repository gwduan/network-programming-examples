#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include "common.h"

#define LISTENQ 1024

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	int on = 1;

	fd_set readfds;

	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

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
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(12345);

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on,
				sizeof(on)) == -1) {
		perror("setsockopt");
		close(sockfd);
		exit(1);
	}

	if (bind(sockfd, (struct sockaddr *)&servaddr,
				sizeof(servaddr)) == -1) {
		perror("bind");
		close(sockfd);
		exit(1);
	}

	if (listen(sockfd, LISTENQ) == -1) {
		perror("listen");
		close(sockfd);
		exit(1);
	}

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		if (select(sockfd + 1, &readfds, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;

			perror("select");
			close(sockfd);
			exit(1);
		}

		if (FD_ISSET(sockfd, &readfds)) {
			peerlen = sizeof(peeraddr);
			if ((connfd = accept(sockfd,
					(struct sockaddr *)&peeraddr,
					&peerlen)) == -1) {
				if (errno == EINTR || errno == EWOULDBLOCK
						|| errno == EAGAIN)
					continue;
 
				perror("accept");
				close(sockfd);
				exit(1);
			}

			if (set_nonblocking(connfd) == -1) {
				perror("set_nonblocking");
				close(connfd);
				continue;
			}

			do_server(connfd);
			close(connfd);
		}
	}

	close(sockfd);

	exit(0);
}
