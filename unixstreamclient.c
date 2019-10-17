#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "common.h"

#define LOCALPATH "/tmp/unixsserver.sock"

int main(void)
{
	int sockfd;
	struct sockaddr_un servaddr;

	if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_LOCAL;
	strncpy(servaddr.sun_path, LOCALPATH, sizeof(servaddr.sun_path));
	servaddr.sun_path[sizeof(servaddr.sun_path)] = '\0';

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
