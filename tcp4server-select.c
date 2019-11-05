#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include "common.h"

#define LISTENQ 1024
#define MAX_FD_SIZE 128
#define BUFF_SIZE 1024

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

static int do_accept(int listenfd, struct conninfo conntab[], int size)
{
	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

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

	return 0;
}

int do_recv(int sockfd, struct conninfo *conn_info)
{
	size_t nleft;
	ssize_t nread;
	char *ptr;

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
			fprintf(stdout, "fd[%d] closed.\n", sockfd);
			conn_info->peer_closed = 1;
			break;
		}

		nleft -= nread;
		ptr += nread;
		conn_info->data_end += nread;
		fprintf(stdout, "fd[%d] recv %ld bytes.\n", sockfd, nread);
	}

	if (conn_info->data_end > conn_info->data_start)
		conn_info->not_empty = 1;

	return 0;
}

int do_send(int sockfd, struct conninfo *conn_info)
{
	size_t nleft;
	ssize_t nwritten;
	char *ptr;
	struct sigaction oact;

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

	return 0;
}

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;
	int on = 1;

	struct conninfo conntab[MAX_FD_SIZE];
	int i;
	fd_set readfds;
	fd_set writefds;
	int maxfd;

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

	for (i = 0; i < MAX_FD_SIZE; i++)
		init_conn_info(conntab + i);

	while (1) {
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(listenfd, &readfds);
		maxfd = listenfd;
		for (i = 0; i < MAX_FD_SIZE; i++) {
			if (!conntab[i].fd)
				continue;

			if (!conntab[i].peer_closed)
				FD_SET(conntab[i].fd, &readfds);

			if (conntab[i].not_empty)
				FD_SET(conntab[i].fd, &writefds);

			if (conntab[i].fd > maxfd)
				maxfd = conntab[i].fd;
		}

		if (select(maxfd + 1, &readfds, &writefds, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;

			perror("select");
			close(listenfd);
			close_all_connfds(conntab, MAX_FD_SIZE);
			exit(1);
		}

		if (FD_ISSET(listenfd, &readfds)) {
			if (do_accept(listenfd, conntab, MAX_FD_SIZE) == -1) {
				close(listenfd);
				close_all_connfds(conntab, MAX_FD_SIZE);
				exit(1);
			}
		}

		for (i = 0; i < maxfd + 1; i++) {
			if (!conntab[i].fd)
				continue;

			if (FD_ISSET(i, &readfds)) {
				if (do_recv(i, conntab + i) == -1) {
					close(i);
					reset_conn_info(conntab + i);
					continue;
				}

				if (conntab[i].peer_closed
						&& !conntab[i].not_empty) {
					fprintf(stdout,
		"fd[%d] peer closed, and no data to send, so close too.\n", i);
					close(i);
					reset_conn_info(conntab + i);
					continue;
				}
			}

			if (FD_ISSET(i, &writefds)) {
				if (do_send(i, conntab + i) == -1) {
					close(i);
					reset_conn_info(conntab + i);
					continue;
				}

				if (conntab[i].peer_closed
						&& !conntab[i].not_empty) {
					fprintf(stdout,
	"fd[%d] peer closed, and all data have been sent, so close.\n", i);
					close(i);
					reset_conn_info(conntab + i);
				}
			}
		}
	}

	close(listenfd);
	close_all_connfds(conntab, MAX_FD_SIZE);

	exit(0);
}
