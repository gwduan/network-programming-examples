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
#define MAX_FD_SIZE MAX_EVENTS + 4
#define BUFF_SIZE 1024
#define TIME_OUT -1

struct conninfo {
	int fd;
	char *buf;
	size_t data_start;
	size_t data_end;
	int not_empty;
	int peer_closed;
};

static void init_conn_info(struct conninfo *conn_info)
{
	conn_info->fd = 0;
	conn_info->buf = NULL;
	conn_info->data_start = 0;
	conn_info->data_end = 0;
	conn_info->not_empty = 0;
	conn_info->peer_closed = 0;

	return;
}

static void reset_conn_info(struct conninfo *conn_info)
{
	conn_info->fd = 0;

	if (conn_info->buf)
		free(conn_info->buf);
	conn_info->buf = NULL;

	conn_info->data_start = 0;
	conn_info->data_end = 0;
	conn_info->not_empty = 0;
	conn_info->peer_closed = 0;

	return;
}

static void close_all_connfds(struct conninfo conntab[], int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!conntab[i].fd)
			continue;

		close(conntab[i].fd);
		reset_conn_info(conntab + i);
	}

	return;
}

static int do_accept(int listenfd, struct conninfo conntab[], int size,
							int epollfd)
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

	if (connfd >= size) {
		fprintf(stderr, "Can't hold new connection any more!\n");
		close(connfd);
		return 0;
	}

	fprintf(stdout, "New connection fd = %d.\n", connfd);

	if (set_nonblocking(connfd) == -1) {
		perror("set_nonblocking");
		close(connfd);
		return -1;
	}

	if (!(conntab[connfd].buf = malloc(BUFF_SIZE))) {
		perror("malloc");
		close(connfd);
		return -1;
	}
	conntab[connfd].fd = connfd;

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = connfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
		perror("epoll_ctl[ADD]");
		return -1;
	}

	return 0;
}

int do_recv(int sockfd, struct conninfo *conn_info, int epollfd)
{
	size_t nleft;
	ssize_t nread;
	char *ptr;
	struct epoll_event ev;

	ptr = conn_info->buf + conn_info->data_end;
	nleft = BUFF_SIZE - conn_info->data_end;
	while (nleft) {
		if ((nread = recv(sockfd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR)
				continue;

			if (errno == EWOULDBLOCK || errno == EAGAIN)
				break;

			perror("recv");
			return -1;
		}
		
		if (!nread) {
			fprintf(stdout, "fd[%d] peer closed.\n", sockfd);
			conn_info->peer_closed = 1;
			break;
		}

		nleft -= nread;
		ptr += nread;
		conn_info->data_end += nread;
		fprintf(stdout, "fd[%d] recv %ld bytes.\n", sockfd, nread);
	}

	if (!nleft) {
		fprintf(stderr, "recv message is too long[max-length:%d]!\n",
								BUFF_SIZE);
		return -1;
	}

	if (conn_info->data_end > conn_info->data_start)
		conn_info->not_empty = 1;

	ev.events = EPOLLOUT | EPOLLET;
	if (!conn_info->peer_closed) {
		ev.events |= EPOLLIN;
	}
	ev.data.fd = sockfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1) {
		perror("epoll_ctl[MOD]");
		return -1;
	}

	return 0;
}

int do_send(int sockfd, struct conninfo *conn_info, int epollfd)
{
	size_t nleft;
	ssize_t nwritten;
	char *ptr;
	struct sigaction oact;
	struct epoll_event ev;

	if (set_sig_handler(SIGPIPE, SIG_IGN, &oact) == -1)
		return -1;

	ptr = conn_info->buf + conn_info->data_start;
	nleft = conn_info->data_end - conn_info->data_start;
	while (nleft) {
		if ((nwritten = send(sockfd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR)
				continue;

			if (errno == EWOULDBLOCK || errno == EAGAIN)
				break;

			perror("send");
			sigaction(SIGPIPE, &oact, NULL);
			return -1;
		}

		nleft -= nwritten;
		ptr += nwritten;
		conn_info->data_start += nwritten;
		fprintf(stdout, "fd[%d] send %ld bytes.\n", sockfd, nwritten);
	}

	if (conn_info->data_start == conn_info->data_end) {
		conn_info->not_empty = 0;
		conn_info->data_start = conn_info->data_end = 0;
	}

	sigaction(SIGPIPE, &oact, NULL);

	ev.events = EPOLLET;
	ev.data.fd = sockfd;
	if (!conn_info->not_empty) {
		if (conn_info->peer_closed) {
			fprintf(stdout, "fd[%d] close.\n", sockfd);
			if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd,
							NULL) == -1) {
				perror("epoll_ctl[DEL]");
			}
			close(sockfd);
			reset_conn_info(conn_info);
		} else {
			ev.events |= EPOLLIN;
			if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,
							&ev) == -1) {
				perror("epoll_ctl[MOD]");
				return -1;
			}
		}
	}

	return 0;
}

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;
	int on = 1;

	struct conninfo conntab[MAX_FD_SIZE];
	int epollfd;
	int nfds;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];
	int i;
	int fd;

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

	for (i = 0; i < MAX_FD_SIZE; i++)
		init_conn_info(conntab + i);

	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
		perror("epoll_ctl[ADD]");
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
			close(listenfd);
			close(epollfd);
			close_all_connfds(conntab, MAX_FD_SIZE);
			exit(1);
		} else if (!nfds) {
			fprintf(stderr, "time out!\n");
			close(listenfd);
			close(epollfd);
			close_all_connfds(conntab, MAX_FD_SIZE);
			exit(1);
		}

		for (i = 0; i < nfds; i++) {
			if (events[i].data.fd == listenfd) {
				if (do_accept(listenfd, conntab, MAX_FD_SIZE,
							epollfd) == -1) {
					close(listenfd);
					close(epollfd);
					close_all_connfds(conntab, MAX_FD_SIZE);
					exit(1);
				}
				continue;
			}

			if (events[i].events & EPOLLIN) {
				fd = events[i].data.fd;
				if (do_recv(fd, conntab + fd, epollfd) == -1) {
					if (epoll_ctl(epollfd, EPOLL_CTL_DEL,
							fd, NULL) == 1) {
						perror("epoll_ctl[DEL]");
					}
					close(fd);
					reset_conn_info(conntab + fd);
					continue;
				}
			}

			if (events[i].events & EPOLLOUT) {
				fd = events[i].data.fd;
				if (do_send(fd, conntab + fd, epollfd) == -1) {
					if (epoll_ctl(epollfd, EPOLL_CTL_DEL,
							fd, NULL) == -1) {
						perror("epoll_ctl[DEL]");
					}
					close(fd);
					reset_conn_info(conntab + fd);
				}
			}
		}
	}

	close(listenfd);
	close(epollfd);
	close_all_connfds(conntab, MAX_FD_SIZE);

	exit(0);
}
