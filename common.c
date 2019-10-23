#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include "common.h"

ssize_t readn(int fd, void *buf, size_t len)
{
	char *ptr;
	size_t nleft;
	ssize_t nread;

	ptr = buf;
	nleft = len;
	while (nleft) {
		if ((nread = recv(fd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR)
				continue;
			else {
				perror("recv");
				return -1;
			}
		} else if (!nread)
			break;

		nleft -= nread;
		ptr += nread;
	}

	return len - nleft;
}

ssize_t writen(int fd, void *buf, size_t len)
{
	char *ptr;
	size_t nleft;
	ssize_t nwritten;

	struct sigaction act, oact;

	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGPIPE, &act, &oact) == -1) {
		perror("sigaction");
		return -1;
	}

	ptr = buf;
	nleft = len;
	while (nleft) {
		if ((nwritten = send(fd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR)
				continue;
			else {
				perror("send");
				sigaction(SIGPIPE, &oact, NULL);
				return -1;
			}
		}

		nleft -= nwritten;
		ptr += nwritten;
	}

	sigaction(SIGPIPE, &oact, NULL);

	return len;
}

int set_nonblocking(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
		perror("fcntl(F_GETFL)");
		return -1;
	}

	if (flags & O_NONBLOCK)
		return 0;

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror("fcntl(F_SETFL)");
		return -1;
	}

	return 0;
}

int set_blocking(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
		perror("fcntl(F_GETFL)");
		return -1;
	}

	if (!(flags & O_NONBLOCK))
		return 0;

	if (fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)) == -1) {
		perror("fcntl(F_SETFL)");
		return -1;
	}

	return 0;
}

ssize_t readn_nonblock(int fd, void *buf, size_t len)
{
	char *ptr;
	size_t nleft;
	ssize_t nread;
	fd_set readset;

	ptr = buf;
	nleft = len;
	while (nleft) {
		FD_ZERO(&readset);
		FD_SET(fd, &readset);
		switch(select(fd + 1, &readset, NULL, NULL, NULL)) {
		case -1:
			if (errno == EINTR)
				continue;

			perror("select");
			return -1;
		case 0:
			fprintf(stderr, "select time out!\n");
			return -1;
		}

		if ((nread = recv(fd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR || errno == EWOULDBLOCK
					|| errno == EAGAIN)
				continue;
			else {
				perror("recv");
				return -1;
			}
		} else if (!nread)
			break;

#ifdef DEBUG
		fprintf(stdout, "%ld recved\n", nread);
#endif
		nleft -= nread;
		ptr += nread;
	}

	return len - nleft;
}

ssize_t writen_nonblock(int fd, void *buf, size_t len)
{
	char *ptr;
	size_t nleft;
	ssize_t nwritten;
	struct sigaction act, oact;
	fd_set writeset;

	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGPIPE, &act, &oact) == -1) {
		perror("sigaction");
		return -1;
	}

	ptr = buf;
	nleft = len;
	while (nleft) {
		FD_ZERO(&writeset);
		FD_SET(fd, &writeset);
		switch(select(fd + 1, NULL, &writeset, NULL, NULL)) {
		case -1:
			if (errno == EINTR)
				continue;

			perror("select");
			sigaction(SIGPIPE, &oact, NULL);
			return -1;
		case 0:
			fprintf(stderr, "time out!\n");
			sigaction(SIGPIPE, &oact, NULL);
			return -1;

		}

		if ((nwritten = send(fd, ptr, nleft, 0)) == -1) {
			if (errno == EINTR || errno == EWOULDBLOCK
					|| errno == EAGAIN)
				continue;
			else {
				perror("send");
				sigaction(SIGPIPE, &oact, NULL);
				return -1;
			}
		}

#ifdef DEBUG
		fprintf(stdout, "%ld sended\n", nwritten);
#endif
		nleft -= nwritten;
		ptr += nwritten;
	}

	sigaction(SIGPIPE, &oact, NULL);

	return len;
}
