#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include "common.h"

#define LISTENQ 1024

#define QUEUE_SIZE 2
#define QUEUE_FULL -1
#define QUEUE_EMPTY -2

#define THREAD_NUMBER 2

struct queue {
	int fds[QUEUE_SIZE + 1];
	int head;
	int tail;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

static void init_queue(struct queue *q)
{
	q->head = q->tail = 0;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);

	return;
}

static int en_queue(struct queue *q, int fd)
{
	pthread_mutex_lock(&q->mutex);
	if (((q->tail + 1) % (QUEUE_SIZE + 1)) == q->head) {
		pthread_mutex_unlock(&q->mutex);
		return QUEUE_FULL;
	}

	q->fds[q->tail] = fd;
	q->tail = (q->tail + 1) % (QUEUE_SIZE + 1);

	fprintf(stdout, "Put fd[%d] to queue.\n", fd);
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);

	return 0;
}

static int de_queue(struct queue *q)
{
	int fd;

	pthread_mutex_lock(&q->mutex);
	while (q->head == q->tail)
		pthread_cond_wait(&q->cond, &q->mutex);

	fd = q->fds[q->head];
	q->head = (q->head + 1) % (QUEUE_SIZE + 1);

	pthread_mutex_unlock(&q->mutex);

	return fd;

}

static void *start_routine(void *arg)
{
	struct queue *fds_queue = arg;
	pthread_t tid = pthread_self();
	int connfd;

	pthread_detach(tid);

	while (1) {
		connfd = de_queue(fds_queue);
		fprintf(stdout, "Thread[%ld] get fd[%d] from queue.\n",
							tid, connfd);
		do_server(connfd);
		close(connfd);
	}

	return 0;
}

int main(void)
{
	int listenfd;
	struct sockaddr_in servaddr;
	int on = 1;

	int connfd;
	struct sockaddr_in peeraddr;
	socklen_t peerlen;

	struct queue fds_queue;
	int i;
	int ret;
	pthread_t tid;

	init_queue(&fds_queue);

	for (i = 0; i < THREAD_NUMBER; i++) {
		if ((ret = pthread_create(&tid, NULL, start_routine,
						(void *)&fds_queue))) {
			errno = ret;
			perror("pthread_create");
			exit(1);
		}
		fprintf(stdout, "[%d]Create thread: %ld.\n", i, tid);
	}

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

		if (en_queue(&fds_queue, connfd) == QUEUE_FULL) {
			fprintf(stderr,
				"queue is full, discard connection[%d]!\n",
				connfd);
			close(connfd);
		}
	}

	close(listenfd);

	exit(0);
}
