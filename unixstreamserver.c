#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include "common.h"

#define LOCALPATH "/tmp/unixsserver.sock"
#define LISTENQ 1024

int main(void)
{
	int sockfd;
	struct sockaddr_un servaddr;

	int connfd;
	struct sockaddr_un peeraddr;
	socklen_t peerlen;

	if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
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
