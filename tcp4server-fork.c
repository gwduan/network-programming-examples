#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <sys/wait.h>
#include "common.h"

#define LISTENQ 1024

static void sigchld_handler(int sig)
{
	int pid;
	int status;
	char buf[256];

	if (sig != SIGCHLD)
		return;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		fprintf(stdout, "Child process [%d] %s.\n", pid,
				status_str(status, buf, sizeof(buf)));
	}

	return;
}

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;
	int on = 1;

	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

	int pid;

	set_sig_handler(SIGCHLD, sigchld_handler, NULL);

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
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

	while (1) {
		peerlen = sizeof(peeraddr);
		if ((connfd = accept(listenfd, (struct sockaddr *)&peeraddr,
						&peerlen)) == -1) {
			if (errno == EINTR)
				continue;
 
			perror("accept");
			close(listenfd);
			exit(1);
		}

		switch (pid = fork()) {
		case -1:
			perror("fork");
			close(connfd);
			continue;
		case 0:
			close(listenfd);
			do_server(connfd);
			close(connfd);
			exit(0);
		default:
			close(connfd);
			fprintf(stdout, "Fork child process[%d].\n", pid);
		}
	}

	close(listenfd);

	exit(0);
}
