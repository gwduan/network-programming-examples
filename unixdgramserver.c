#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include "common.h"

#define LOCALPATH "/tmp/unixdserver.sock"

int main(void)
{
	int sockfd;
	struct sockaddr_un servaddr;

	struct sockaddr_un peeraddr;
	socklen_t peerlen;

	char recvbuf[1024 + 1];
	char sendbuf[1024 + 4 + 1];
	ssize_t nread;
	int ret;

	if ((sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (unlink(LOCALPATH) == -1) {
		if (errno != ENOENT) {
			perror("unlink");
			close(sockfd);
			exit(1);
		}
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_LOCAL;
	strncpy(servaddr.sun_path, LOCALPATH, sizeof(servaddr.sun_path));
	servaddr.sun_path[sizeof(servaddr.sun_path)] = '\0';

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
			fprintf(stderr, "%d\n", errno);
			close(sockfd);
			exit(1);

		}
		fprintf(stdout, "send %d bytes: %s\n", ret, sendbuf);
	}

	close(sockfd);

	exit(0);
}
