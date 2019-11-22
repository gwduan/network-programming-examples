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
	struct sockaddr_in6 servaddr;
	int on = 1;

	int connfd;
	struct sockaddr_in6 peeraddr;
	socklen_t peerlen;

	if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin6_family = AF_INET6;
	servaddr.sin6_addr = in6addr_any;
	servaddr.sin6_port = htons(12345);

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
		peerlen = sizeof(peeraddr);
		if ((connfd = accept(sockfd, (struct sockaddr *)&peeraddr,
						&peerlen)) == -1) {
			if (errno == EINTR)
				continue;
 
			perror("accept");
			close(sockfd);
			exit(1);
		}

		do_server(connfd);
		close(connfd);
	}

	close(sockfd);

	exit(0);
}
