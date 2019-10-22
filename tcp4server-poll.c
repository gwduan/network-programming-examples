#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <poll.h>
#include "common.h"

#define LISTENQ 1024
#define INIT_SIZE 128
#define TIME_OUT -1

void close_all_fds(struct pollfd *fds, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (fds[i].fd != -1)
			close(fds[i].fd);
	}

	return;
}

int do_accept(int sockfd, struct pollfd *fds, int size)
{
	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	int i;

	peerlen = sizeof(peeraddr);
	if ((connfd = accept(sockfd, (struct sockaddr *)&peeraddr,
					&peerlen)) == -1) {
		if (errno == EINTR)
			return 0;

		perror("accept");
		return -1;
	}

	fprintf(stdout, "New connection fd = %d.\n", connfd);

	for (i = 1; i < size; i++) {
		if (fds[i].fd == -1) {
			fds[i].fd = connfd;
			fds[i].events = POLLRDNORM;
			break;
		}
	}

	if (i == size) {
		fprintf(stderr, "Can't hold new connection any more!\n");
		close(connfd);
	}

	return 0;
}

int do_echo(int no, struct pollfd *fds)
{
	char buf[1024];
	int nread;

	if ((nread = recv(fds[no].fd, buf, sizeof(buf), 0)) == -1) {
		if (errno == EINTR)
			return 0;

		perror("recv");
		close(fds[no].fd);
		fds[no].fd = -1;
		return -1;
	}

	if (!nread) {
		fprintf(stdout, "Peer[%d] closed.\n", fds[no].fd);
		close(fds[no].fd);
		fds[no].fd = -1;
		return 0;
	}

	buf[nread] = '\0';
	fprintf(stdout, "recv from peer[%d]: %s\n", fds[no].fd, buf);

	if (writen(fds[no].fd, buf, nread) == -1) {
		fprintf(stderr, "send to peer[%d] error!\n", fds[no].fd);
		close(fds[no].fd);
		fds[no].fd = -1;
		return -1;
	}

	fprintf(stdout, "send to peer[%d]: %s\n", fds[no].fd, buf);

	return 0;
}

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	int on = 1;

	struct pollfd fds[INIT_SIZE];
	int i;
	int ready_num;



	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
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

	fds[0].fd = sockfd;
	fds[0].events = POLLRDNORM;
	for (i = 1; i < INIT_SIZE; i++) {
		fds[i].fd = -1;
	}

	while (1) {
		if ((ready_num = poll(fds, INIT_SIZE, TIME_OUT)) == -1) {
			if (errno == EINTR)
				continue;

			perror("poll");
			close_all_fds(fds, INIT_SIZE);
			exit(1);
		}
		
		if (!ready_num) {
			fprintf(stderr, "time out!\n");
			close_all_fds(fds, INIT_SIZE);
			exit(1);
		}

		if (fds[0].revents & POLLRDNORM) {
			if (do_accept(fds[0].fd, fds, INIT_SIZE) == -1) {
				close_all_fds(fds, INIT_SIZE);
				exit(1);
			}

			if (--ready_num == 0)
				continue;
		}

		for (i = 1; i < INIT_SIZE; i++) {
			if (fds[i].fd == -1)
				continue;

			if (fds[i].revents & (POLLRDNORM | POLLERR)) {
				(void) do_echo(i, fds);

				if (--ready_num == 0)
					break;
			}
		}
	}

	close_all_fds(fds, INIT_SIZE);

	exit(0);
}
