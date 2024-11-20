#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include "common.h"

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;

	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	char recvbuf[1024 + 1];
	char sendbuf[1024 + 4 + 1];
	ssize_t nread;
	int ret;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(12345);

	if (bind(sockfd, (struct sockaddr *)&servaddr,
				sizeof(servaddr)) == -1) {
		perror("bind");
		close(sockfd);
		exit(1);
	}

	while (1) {
		peerlen = sizeof(peeraddr);
		if ((nread = recvfrom(sockfd, recvbuf, 1024, 0,
			(struct sockaddr *)&peeraddr, &peerlen)) == -1) {
			perror("recvfrom");
			close(sockfd);
			exit(1);
		}
		recvbuf[nread] = '\0';
		fprintf(stdout, "received %ld bytes: %s\n", nread, recvbuf);

		snprintf(sendbuf, sizeof(sendbuf), "Hi, %s", recvbuf);
		if ((ret = sendto(sockfd, sendbuf, strlen(sendbuf), 0,
				(struct sockaddr *)&peeraddr, peerlen)) == -1) {
			perror("sendto");
			close(sockfd);
			exit(1);

		}
		fprintf(stdout, "send %d bytes: %s\n", ret, sendbuf);
	}

	close(sockfd);

	exit(0);
}
