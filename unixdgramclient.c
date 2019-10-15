#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "common.h"

#define LOCALPATH "/tmp/unixdserver.sock"

int main(void)
{
	int sockfd;
	struct sockaddr_un clieaddr;
	struct sockaddr_un servaddr;
	socklen_t servlen = sizeof(servaddr);
	char tmpfile[] = "/tmp/unixdclientXXXXXX";

	struct sockaddr_un replyaddr;
	socklen_t replylen;

	char sendbuf[1024 + 1];
	char recvbuf[1024 + 1];
	int ret;
	ssize_t nread;

	if ((sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (mkstemp(tmpfile) == -1) {
		perror("mkstemp");
		close(sockfd);
		exit(1);
	}
	if (unlink(tmpfile) == -1) {
		perror("unlink");
		close(sockfd);
		exit(1);
	}

	bzero(&clieaddr, sizeof(clieaddr));
	clieaddr.sun_family = AF_LOCAL;
	strncpy(clieaddr.sun_path, tmpfile, sizeof(clieaddr.sun_path));
	clieaddr.sun_path[sizeof(clieaddr.sun_path)] = '\0';

	if (bind(sockfd, (struct sockaddr *)&clieaddr,
				sizeof(clieaddr)) == -1) {
		perror("bind");
		close(sockfd);
		unlink(tmpfile);
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_LOCAL;
	strncpy(servaddr.sun_path, LOCALPATH, sizeof(servaddr.sun_path));
	servaddr.sun_path[sizeof(servaddr.sun_path)] = '\0';

	while (fgets(sendbuf, sizeof(sendbuf), stdin)) {
		if (strlen(sendbuf) == 1 && sendbuf[0] == '\n') {
			fprintf(stdout, "Bye bye.\n");
			break;
		}

		if ((ret = sendto(sockfd, sendbuf, strlen(sendbuf), 0,
				(struct sockaddr *)&servaddr, servlen)) == -1) {
			perror("sendto");
			close(sockfd);
			unlink(tmpfile);
			exit(1);
		}
		fprintf(stdout, "send %d bytes: %s\n", ret, sendbuf);

		replylen = sizeof(replyaddr);
		if ((nread = recvfrom(sockfd, recvbuf, 1024, 0,
			(struct sockaddr *)&replyaddr, &replylen)) == -1) {
			perror("recvfrom");
			close(sockfd);
			unlink(tmpfile);
			exit(1);
		}
		recvbuf[nread] = '\0';
		fprintf(stdout, "received %ld bytes: %s\n", nread, recvbuf);
	}

	close(sockfd);
	unlink(tmpfile);

	exit(0);
}
