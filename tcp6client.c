#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include "common.h"

#define SERVER_IP "::1"
#define SERVER_PORT 12345

int main(void)
{
	int sockfd;
	struct sockaddr_in6 servaddr;
	int ret;

	if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin6_family = AF_INET6;
	if (!(ret = inet_pton(AF_INET6, SERVER_IP, &(servaddr.sin6_addr)))) {
		fprintf(stderr, "bad address: [%s]!\n", SERVER_IP);
		close(sockfd);
		exit(1);
	} else if (ret == -1) {
		perror("inet_pton");
		close(sockfd);
		exit(1);
	}
	servaddr.sin6_port = htons(SERVER_PORT);

	if (connect(sockfd, (struct sockaddr *)&servaddr,
				sizeof(servaddr)) == -1) {
		perror("connect");
		close(sockfd);
		exit(1);
	}

	do_client(sockfd);
	close(sockfd);

	exit(0);
}
