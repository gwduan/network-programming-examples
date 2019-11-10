#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include "common.h"

#define LISTENQ 1024

static void *start_routine(void *arg)
{
	int connfd = (int)(long)arg;

	pthread_detach(pthread_self());

	do_server(connfd);

	close(connfd);

	return 0;
}

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;
	int on = 1;

	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

	pthread_t tid;
	int ret;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(12345);

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on,
				sizeof(on)) == -1) {
		perror("setsockopt");
		close(listenfd);
		exit(1);
	}

	if (bind(listenfd, (struct sockaddr *)&servaddr,
				sizeof(servaddr)) == -1) {
		perror("bind");
		close(listenfd);
		exit(1);
	}

	if (listen(listenfd, LISTENQ) == -1) {
		perror("listen");
		close(listenfd);
		exit(1);
	}

	while (1) {
		peerlen = sizeof(peeraddr);
		if ((connfd = accept(listenfd, (struct sockaddr *)&peeraddr,
						&peerlen)) == -1) {
			if (errno == EINTR)
				continue;
 
			perror("accept");
			close(listenfd);
			exit(1);
		}

		if ((ret = pthread_create(&tid, NULL, start_routine,
						(void *)(long)connfd))) {
			errno = ret;
			perror("pthread_create");
			close(connfd);
			continue;
		}

		fprintf(stdout, "Create thread[%ld].\n", tid);
	}

	close(listenfd);

	exit(0);
}
