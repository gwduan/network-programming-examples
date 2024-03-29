#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
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

	struct sigaction oact;

	if (set_sig_handler(SIGPIPE, SIG_IGN, &oact) == -1)
		return -1;

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
			fprintf(stderr, "time out!\n");
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
	struct sigaction oact;
	fd_set writeset;

	if (set_sig_handler(SIGPIPE, SIG_IGN, &oact) == -1)
		return -1;

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

ssize_t readn_nonblock_timeout(int fd, void *buf, size_t len, struct timeval *timeout)
{
	char *ptr;
	size_t nleft;
	ssize_t nread;
	fd_set readset;
	long total_usec;
	struct timeval tv_start, tv_end, tv_curr;
	int ret;

	if (!timeout)
		return readn_nonblock(fd, buf, len);

	total_usec = timeout->tv_sec * 1000000 + timeout->tv_usec;
	tv_curr = *timeout;
#ifdef DEBUG
	fprintf(stdout, "initial timeout: %ld\n", total_usec);
#endif

	ptr = buf;
	nleft = len;
	while (nleft) {
		FD_ZERO(&readset);
		FD_SET(fd, &readset);

		gettimeofday(&tv_start, NULL);
		ret = select(fd + 1, &readset, NULL, NULL, &tv_curr);
		gettimeofday(&tv_end, NULL);

		total_usec -= (tv_end.tv_sec * 1000000 + tv_end.tv_usec)
			- (tv_start.tv_sec * 1000000 + tv_start.tv_usec);
#ifdef DEBUG
		fprintf(stdout, "after select, remain time: %ld\n", total_usec);
#endif
		if (total_usec < 0)
			total_usec = 0;
		tv_curr.tv_sec = total_usec / 1000000;
		tv_curr.tv_usec = total_usec % 1000000;

		switch(ret) {
		case -1:
			if (errno == EINTR)
				continue;

			perror("select");
			return -1;
		case 0:
			fprintf(stderr, "time out!\n");
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

ssize_t writen_nonblock_timeout(int fd, void *buf, size_t len, struct timeval *timeout)
{
	char *ptr;
	size_t nleft;
	ssize_t nwritten;
	struct sigaction oact;
	fd_set writeset;
	long total_usec;
	struct timeval tv_start, tv_end, tv_curr;
	int ret;

	if (!timeout)
		return writen_nonblock(fd, buf, len);

	if (set_sig_handler(SIGPIPE, SIG_IGN, &oact) == -1)
		return -1;

	total_usec = timeout->tv_sec * 1000000 + timeout->tv_usec;
	tv_curr = *timeout;
#ifdef DEBUG
	fprintf(stdout, "initial timeout: %ld\n", total_usec);
#endif

	ptr = buf;
	nleft = len;
	while (nleft) {
		FD_ZERO(&writeset);
		FD_SET(fd, &writeset);

		gettimeofday(&tv_start, NULL);
		ret = select(fd + 1, NULL, &writeset, NULL, &tv_curr);
		gettimeofday(&tv_end, NULL);

		total_usec -= (tv_end.tv_sec * 1000000 + tv_end.tv_usec)
			- (tv_start.tv_sec * 1000000 + tv_start.tv_usec);
#ifdef DEBUG
		fprintf(stdout, "after select, remain time: %ld\n", total_usec);
#endif
		if (total_usec < 0)
			total_usec = 0;
		tv_curr.tv_sec = total_usec / 1000000;
		tv_curr.tv_usec = total_usec % 1000000;

		switch(ret) {
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

int send_fd(int sockfd, int sendfd)
{
	struct sigaction oact;
	struct msghdr msg;
	char iobuf[1];
	struct iovec iov;
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct cmsghdr *cmsg;

	if (set_sig_handler(SIGPIPE, SIG_IGN, &oact) == -1)
		return -1;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov.iov_base = iobuf;
	iov.iov_len = sizeof(iobuf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	*((int *)CMSG_DATA(cmsg)) = sendfd;

	while (1) {
		if (sendmsg(sockfd, &msg, 0) == -1) {
			if (errno == EINTR)
				continue;

			perror("sendmsg");
			sigaction(SIGPIPE, &oact, NULL);
			return -1;
		}

		break;
	}

	sigaction(SIGPIPE, &oact, NULL);

	return 0;
}

int recv_fd(int sockfd, int *recvfd)
{
	struct msghdr msg;
	char iobuf[1];
	struct iovec iov;
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct cmsghdr *cmsg;
	int ret;


	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov.iov_base = iobuf;
	iov.iov_len = sizeof(iobuf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);

	while (1) {
		if ((ret = recvmsg(sockfd, &msg, 0)) == -1) {
			if (errno == EINTR)
				continue;

			perror("recvmsg");
			return -1;
		} else if (!ret) {
			fprintf(stderr, "peer closed!");
			return -1;
		}

		break;
	}

	if ((cmsg = CMSG_FIRSTHDR(&msg))
			&& cmsg->cmsg_level == SOL_SOCKET
			&& cmsg->cmsg_type == SCM_RIGHTS
			&& cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
		*recvfd = *((int *)CMSG_DATA(cmsg));
	} else {
		fprintf(stderr, "send format wrong!");
		return -1;
	}

	return 0;
}

char *status_str(int status, char *buf, size_t len)
{
	if (!buf || !len)
		return NULL;

	if (WIFEXITED(status)) {
		snprintf(buf, len, "exited, status=%d", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		snprintf(buf, len, "killed by signal %d%s",
				WTERMSIG(status),
#ifdef WCOREDUMP
				WCOREDUMP(status) ? " (core dump)" : ""
#else
				""
#endif
				);
	} else if (WIFSTOPPED(status)) {
		snprintf(buf, len, "stopped by signal %d", WSTOPSIG(status));
	} else if (WIFCONTINUED(status)) {
		snprintf(buf, len, "continued");
	}

	return buf;
}

int set_sig_handler(int sig, void (*func)(int), struct sigaction *oact)
{
	struct sigaction act;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(sig, &act, oact) == -1) {
		perror("sigaction");
		return -1;
	}

	return 0;
}
