#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

int do_server(int connfd)
{
	int recvfd;
	FILE *fp;
	char buf[256];

	if (recv_fd(connfd, &recvfd) == -1) {
		fprintf(stderr, "recv_fd fail!\n");
		return -1;
	}

	if (!(fp = fdopen(recvfd, "r"))) {
		perror("fdopen");
		close(recvfd);
		return -1;
	}

	fprintf(stdout, "read from recvfd:\n-----BEGIN-----\n");

	while (fgets(buf, sizeof(buf), fp))
		fputs(buf, stdout);

	fprintf(stdout, "\n-----END-----\n");

	fclose(fp);

	return 0;
}

int do_client(int connfd)
{
	int sendfd;

	if ((sendfd = open("/etc/hosts", O_RDONLY)) == -1) {
		perror("open");
		return -1;
	}

	if (send_fd(connfd, sendfd) == -1) {
		fprintf(stderr, "send_fd fail!\n");
		return -1;
	}

	close(sendfd);
	fprintf(stdout, "send fd OK.\n");

	return 0;
}
