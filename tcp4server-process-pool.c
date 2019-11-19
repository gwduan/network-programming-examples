#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include "common.h"

#define LISTENQ 1024
#define POOL_SIZE 4

struct poolinfo {
	int pid;
	int fd;
	int restart;
};

static struct poolinfo pooltab[POOL_SIZE];

static int do_child(int sockfd)
{
	sigset_t mask;
	int connfd;

	set_sig_handler(SIGCHLD, SIG_DFL, NULL);
	set_sig_handler(SIGTERM, SIG_DFL, NULL);
	set_sig_handler(SIGINT, SIG_DFL, NULL);

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	while (1) {
		if (recv_fd(sockfd, &connfd) == -1)
			exit(1);

		do_server(connfd);
		close(connfd);
	}

	return 0;
}

static int start_one_child(int index, struct poolinfo  *pool_info)
{
	int sv[2];
	int pid;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
		perror("socketpair");
		return -1;
	}

	switch (pid = fork()) {
	case -1:
		perror("fork");
		close(sv[0]);
		close(sv[1]);
		return -1;
	case 0:
		close(sv[0]);
		do_child(sv[1]);
		exit(0);
	default:
		pool_info->pid = pid;
		pool_info->fd = sv[0];
		pool_info->restart = 1;
		fprintf(stdout,
			"Start child[%d] process, pid: [%d], fd: [%d]->[%d].\n",
			index, pid, sv[0], sv[1]);
		close(sv[1]);
	}

	return 0;
}

static int start_all_children(struct poolinfo pooltab[], int size)
{
	sigset_t mask, omask;
	int i;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	for (i = 0; i < size; i++) {
		if (start_one_child(i, pooltab + i) == -1) {
			sigprocmask(SIG_SETMASK, &omask, NULL);
			return -1;
		}
	}

	sigprocmask(SIG_SETMASK, &omask, NULL);

	return 0;
}

static void stop_all_children(struct poolinfo pooltab[], int size)
{
	sigset_t mask, omask;
	int i;

	fprintf(stdout, "Shutdown all children, please wait...\n");

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	for (i = 0; i < size; i++) {
		if (!pooltab[i].pid)
			continue;

		fprintf(stdout, "Stop child[%d] process, pid=[%d].\n",
						i, pooltab[i].pid);
		pooltab[i].restart = 0;
		if (kill(pooltab[i].pid, SIGTERM) == -1)
			perror("kill");
	}

	sigprocmask(SIG_SETMASK, &omask, NULL);

	sleep(1);
	for (i = 0; i < size; i++) {
		if (!pooltab[i].pid)
			continue;

		if (!kill(pooltab[i].pid, 0)) {
			fprintf(stdout,
				"Force stop child[%d] process, pid=[%d].\n",
				i, pooltab[i].pid);
			kill(pooltab[i].pid, SIGKILL);
		}
		close(pooltab[i].fd);
	}

	return;
}

static void sigchld_handler(int sig)
{
	int pid;
	int status;
	char buf[256];
	int i;

	if (sig != SIGCHLD)
		return;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		fprintf(stdout, "Child process [%d] %s.\n", pid,
				status_str(status, buf, sizeof(buf)));

		for (i = 0; i < POOL_SIZE; i++) {
			if (pooltab[i].pid != pid)
				continue;

			close(pooltab[i].fd);
			if (pooltab[i].restart)
				start_one_child(i, pooltab + i);
			break;
		}
	}

	return;
}

static void sigterm_handler(int sig)
{
	if (sig != SIGTERM && sig != SIGINT)
		return;

	stop_all_children(pooltab, POOL_SIZE);

	fprintf(stdout, "Exit now.\n");

	exit(0);
}

int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	int on = 1;

	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

	static int requests = 0;
	int child_id;

	set_sig_handler(SIGCHLD, sigchld_handler, NULL);
	set_sig_handler(SIGTERM, sigterm_handler, NULL);
	set_sig_handler(SIGINT, sigterm_handler, NULL);

	if (start_all_children(pooltab, POOL_SIZE) == -1) {
		fprintf(stderr, "init process poll error!\n");
		stop_all_children(pooltab, POOL_SIZE);
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		stop_all_children(pooltab, POOL_SIZE);
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(12345);

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on,
				sizeof(on)) == -1) {
		perror("setsockopt");
		close(sockfd);
		stop_all_children(pooltab, POOL_SIZE);
		exit(1);
	}

	if (bind(sockfd, (struct sockaddr *)&servaddr,
				sizeof(servaddr)) == -1) {
		perror("bind");
		close(sockfd);
		stop_all_children(pooltab, POOL_SIZE);
		exit(1);
	}

	if (listen(sockfd, LISTENQ) == -1) {
		perror("listen");
		close(sockfd);
		stop_all_children(pooltab, POOL_SIZE);
		exit(1);
	}

	while (1) {
		peerlen = sizeof(peeraddr);
		if ((connfd = accept(sockfd, (struct sockaddr *)&peeraddr,
						&peerlen)) == -1) {
			if (errno == EINTR)
				continue;

			perror("accept");
			close(sockfd);
			stop_all_children(pooltab, POOL_SIZE);
			exit(1);
		}

		child_id = requests % POOL_SIZE;
		fprintf(stdout, "Request[%d] transfer to child[%d].\n",
						requests, child_id);
		if (send_fd(pooltab[child_id].fd, connfd) == -1)
			fprintf(stderr, "send fd[%d] to child[%d] error!\n",
							connfd, child_id);

		close(connfd);
		requests++;
	}

	close(sockfd);
	stop_all_children(pooltab, POOL_SIZE);

	exit(0);
}
