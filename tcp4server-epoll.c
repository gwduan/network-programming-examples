#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <sys/epoll.h>
#include <string.h>
#include <signal.h>
#include "common.h"

#define LISTENQ 1024
#define MAX_EVENTS 128
#define TIME_OUT -1

static int do_accept(int listenfd, int epollfd)
{
	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;
	struct epoll_event ev;

	peerlen = sizeof(peeraddr);
	if ((connfd = accept(listenfd, (struct sockaddr *)&peeraddr,
					&peerlen)) == -1) {
		if (errno == EINTR)
			return 0;

		perror("accept");
		return -1;
	}

	fprintf(stdout, "New connection fd = %d.\n", connfd);

	if (set_nonblocking(connfd) == -1) {
		perror("set_nonblocking");
		close(connfd);
		return -1;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = connfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
		perror("epoll_ctl: add conn fd");
		close(connfd);
		return -1;
	}

	return 0;
}

static int do_echo(int sockfd)
{
	char buf[1024 + 1];
	size_t nleft;
	ssize_t nread;
	ssize_t nwritten;
	char *ptr;
	int peer_closed = 0;
	struct sigaction oact;

	ptr = buf;
	nleft = sizeof(buf);
	while (nleft) {
		if ((nread = recv(sockfd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR)
				continue;

			if (errno == EWOULDBLOCK || errno == EAGAIN)
				break;

			perror("recv");
			close(sockfd);
			return -1;
		}
		
		if (!nread) {
			fprintf(stdout, "Peer[%d] closed.\n", sockfd);
			if (nleft == sizeof(buf)) {
				/* recv nothing */
				close(sockfd);
				return 0;
			}

			peer_closed = 1;
			break;
		}

		nleft -= nread;
		ptr += nread;
	}

	if (!nleft) {
		fprintf(stderr, "recv message is too long[max-length:%ld]!\n",
							sizeof(buf) - 1);
		close(sockfd);
		return -1;
	}

	buf[sizeof(buf) - nleft] = '\0';
	fprintf(stdout, "fd[%d] recv message: [%s].\n", sockfd, buf);

	if (set_sig_handler(SIGPIPE, SIG_IGN, &oact) == -1) {
		close(sockfd);
		return -1;
	}

	ptr = buf;
	nleft = strlen(buf);
	fprintf(stdout, "fd[%d] send message: [", sockfd);
	while (nleft) {
		if ((nwritten = send(sockfd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR)
				continue;

			if (errno == EWOULDBLOCK || errno == EAGAIN)
				break;

			perror("send");
			sigaction(SIGPIPE, &oact, NULL);
			close(sockfd);
			return -1;
		}

		fprintf(stdout, "%*.*s", (int)nwritten, (int)nwritten, ptr);
		nleft -= nwritten;
		ptr += nwritten;
	}
	fprintf(stdout, "].\n");

	sigaction(SIGPIPE, &oact, NULL);

	if (nleft)
		fprintf(stderr, "remain message discard[%ld bytes]!\n", nleft);

	if (peer_closed)
		close(sockfd);

	return 0;
}

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;
	int on = 1;

	int epollfd;
	int nfds;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];
	int i;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (set_nonblocking(listenfd) == -1) {
		perror("set_nonblocking");
		close(listenfd);
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

	if ((epollfd = epoll_create1(0)) == -1) {
		perror("epoll_create1");
		close(listenfd);
		exit(1);
	}

	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
		perror("epoll_ctl: add listen fd");
		close(listenfd);
		close(epollfd);
		exit(1);
	}

	while (1) {
		if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS,
						TIME_OUT)) == -1) {
			if (errno == EINTR)
				continue;

			perror("epoll_wait");
			/* cleanup */
			exit(1);
		} else if (!nfds) {
			fprintf(stderr, "time out!\n");
			/* cleanup */
			exit(1);
		}

		for (i = 0; i < nfds; i++) {
			if (events[i].data.fd == listenfd) {
				if (do_accept(listenfd, epollfd) == -1) {
					/* cleanup */
					exit(1);
				}
				continue;
			}

			(void) do_echo(events[i].data.fd);
		}
	}

	/* cleanup */
	exit(0);
}
