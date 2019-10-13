#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
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
