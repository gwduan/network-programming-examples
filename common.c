#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	struct sigaction act, oact;
	fd_set writeset;
	long total_usec;
	struct timeval tv_start, tv_end, tv_curr;
	int ret;

	if (!timeout)
		return writen_nonblock(fd, buf, len);

	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGPIPE, &act, &oact) == -1) {
		perror("sigaction");
		return -1;
	}

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
	struct msghdr msg;
	char iobuf[1];
	struct iovec iov;
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct cmsghdr *cmsg;


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
			return -1;
		}

		break;
	}

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


	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov.iov_base = iobuf;
	iov.iov_len = sizeof(iobuf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);

	while (1) {
		if (recvmsg(sockfd, &msg, 0) == -1) {
			if (errno == EINTR)
				continue;

			perror("recvmsg");
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

int do_recv_send(int connfd)
{
	char buf[1024 + 4 + 1];
	int len;
	int nread;

	len = 4;
	if ((nread = readn(connfd, buf, len)) == -1) {
		fprintf(stderr, "recv error!\n");
		return -1;
	} else if (nread != len) {
		fprintf(stderr, "recv message format is wrong!\n");
		return -1;
	}
	buf[4] = '\0';
	len = atoi(buf);
	if (len > 1024) {
		fprintf(stderr, "max length of content is 1024!\n");
		return -1;
	}
	fprintf(stdout, "recv header = [%s].\n", buf);

	if ((nread = readn(connfd, buf + 4, len)) == -1) {
		fprintf(stderr, "readn error!\n");
		return -1;
	}
	buf[4 + nread] = '\0';
	fprintf(stdout, "recv content[%d] = [%s].\n", nread, buf + 4);

	if (writen(connfd, buf, len + 4) == -1) {
		fprintf(stderr, "send error!\n");
		return -1;
	}
	fprintf(stdout, "send message[%d] = [%s].\n", len + 4, buf);

	return 0;
}

int do_send_recv(int connfd)
{
	char buf[1024 + 4 + 1];
	char tmp[5];
	int len;
	int nread;
	char *snd_msg = "hello, world!";

	strncpy(buf + 4, snd_msg, sizeof(buf) - 4 - 1);
	buf[sizeof(buf) - 1] = '\0';
	len = strlen(buf + 4);
	snprintf(tmp, sizeof(tmp), "%04d", (int)strlen(buf + 4));
	strncpy(buf, tmp, 4);
	len += 4;

	if (writen(connfd, buf, len) == -1) {
		fprintf(stderr, "send message error!\n");
		return -1;
	}
	fprintf(stdout, "send message[%d] = [%s].\n", len, buf);

	len = 4;
	if ((nread = readn(connfd, buf, len)) == -1) {
		fprintf(stderr, "recv error!\n");
		return -1;
	} else if (nread != len) {
		fprintf(stderr, "recv message format is wrong!\n");
		return -1;
	}
	buf[4] = '\0';
	len = atoi(buf);
	if (len > 1024) {
		fprintf(stderr, "max length of content is 1024!\n");
		return -1;
	}
	fprintf(stdout, "recv header = [%s].\n", buf);

	if ((nread = readn(connfd, buf + 4, len)) == -1) {
		fprintf(stderr, "recv error!\n");
		return -1;
	}
	buf[4 + nread] = '\0';
	fprintf(stdout, "recv content[%d] = [%s].\n", nread, buf + 4);

	return 0;
}

int do_recv_send_nonblock(int connfd)
{
	char buf[1024 + 4 + 1];
	int len;
	int nread;
	struct timeval timeout;

	len = 4;
	if ((nread = readn_nonblock(connfd, buf, len)) == -1) {
		fprintf(stderr, "recv error!\n");
		return -1;
	} else if (nread != len) {
		fprintf(stderr, "recv message format is wrong!\n");
		return -1;
	}
	buf[4] = '\0';
	len = atoi(buf);
	if (len > 1024) {
		fprintf(stderr, "max length of content is 1024!\n");
		return -1;
	}
	fprintf(stdout, "recv header = [%s].\n", buf);

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if ((nread = readn_nonblock_timeout(connfd, buf + 4, len,
					&timeout)) == -1) {
		fprintf(stderr, "readn_nonblock_timeout error!\n");
		return -1;
	}
	buf[4 + nread] = '\0';
	fprintf(stdout, "recv content[%d] = [%s].\n", nread, buf + 4);

	if (writen_nonblock_timeout(connfd, buf, len + 4, &timeout) == -1) {
		fprintf(stderr, "send error!\n");
		return -1;
	}
	fprintf(stdout, "send message[%d] = [%s].\n", len + 4, buf);

	return 0;
}

int do_send_recv_nonblock(int connfd)
{
	char buf[1024 + 4 + 1];
	char tmp[5];
	int len;
	int nread;
	char *snd_msg = "hello, world!";
	struct timeval timeout;

	strncpy(buf + 4, snd_msg, sizeof(buf) - 4 - 1);
	buf[sizeof(buf) - 1] = '\0';
	len = strlen(buf + 4);
	snprintf(tmp, sizeof(tmp), "%04d", (int)strlen(buf + 4));
	strncpy(buf, tmp, 4);
	len += 4;

	if (writen_nonblock(connfd, buf, len) == -1) {
		fprintf(stderr, "send message error!\n");
		return -1;
	}
	fprintf(stdout, "send message[%d] = [%s].\n", len, buf);

	len = 4;
	if ((nread = readn_nonblock(connfd, buf, len)) == -1) {
		fprintf(stderr, "recv error!\n");
		return -1;
	} else if (nread != len) {
		fprintf(stderr, "recv message format is wrong!\n");
		return -1;
	}
	buf[4] = '\0';
	len = atoi(buf);
	if (len > 1024) {
		fprintf(stderr, "max length of content is 1024!\n");
		return -1;
	}
	fprintf(stdout, "recv header = [%s].\n", buf);

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if ((nread = readn_nonblock_timeout(connfd, buf + 4, len,
					&timeout)) == -1) {
		fprintf(stderr, "recv error!\n");
		return -1;
	}
	buf[4 + nread] = '\0';
	fprintf(stdout, "recv content[%d] = [%s].\n", nread, buf + 4);

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
