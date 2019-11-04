#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"

static void sigchld_handler(int sig)
{
	int pid;
	int status;
	char buf[256];

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		fprintf(stdout, "pid[%d] %s.\n", pid,
				status_str(status, buf, sizeof(buf)));
	}

	return;
}

int do_server(int connfd)
{
	static int first = 1;
	struct sigaction act;
	int pid;

	if (first) {
		act.sa_handler = sigchld_handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		if (sigaction(SIGCHLD, &act, NULL) == -1) {
			perror("sigaction");
			return -1;
		}

		first = 0;
	}

	switch (pid = fork()) {
	case -1:
		perror("fork");
		return -1;
	case 0:
		do_recv_send(connfd);
		close(connfd);
		exit(0);
	default:
		close(connfd);
		fprintf(stdout, "fork process[%d].\n", pid);
	}

	return 0;
}

int do_client(int connfd)
{
	return do_send_recv(connfd);
}
