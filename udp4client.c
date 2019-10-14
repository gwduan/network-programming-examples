#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include "common.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	socklen_t servlen = sizeof(servaddr);

	struct sockaddr_in replyaddr;
	socklen_t replylen;

	char sendbuf[1024 + 1];
	char recvbuf[1024 + 1];
	int ret;
	ssize_t nread;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
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

	while (fgets(sendbuf, sizeof(sendbuf), stdin)) {
		if ((ret = sendto(sockfd, sendbuf, strlen(sendbuf), 0,
				(struct sockaddr *)&servaddr, servlen)) == -1) {
			perror("sendto");
			close(sockfd);
			exit(1);
		}
		fprintf(stdout, "send %d bytes: %s\n", ret, sendbuf);

		replylen = sizeof(replyaddr);
		if ((nread = recvfrom(sockfd, recvbuf, 1024, 0,
			(struct sockaddr *)&replyaddr, &replylen)) == -1) {
			perror("recvfrom");
			close(sockfd);
			exit(1);
		}
		recvbuf[nread] = '\0';
		fprintf(stdout, "received %ld bytes: %s\n", nread, recvbuf);
	}

	close(sockfd);

	exit(0);
}
