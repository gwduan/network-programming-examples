#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

int do_server(int connfd)
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

int do_client(int connfd)
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
