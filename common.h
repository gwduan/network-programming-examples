#ifndef GWDUAN_COMMON_H
#define GWDUAN_COMMON_H

extern ssize_t readn(int fd, void *buf, size_t len);
extern ssize_t writen(int fd, void *buf, size_t len);

extern int do_send_recv(int connfd);
extern int do_send_recv_nonblock(int connfd);
extern int do_recv_send(int connfd);
extern int do_recv_send_nonblock(int connfd);
extern int do_server(int connfd);
extern int do_client(int connfd);

extern int set_nonblocking(int fd);
extern int set_blocking(int fd);

extern ssize_t readn_nonblock(int fd, void *buf, size_t len);
extern ssize_t writen_nonblock(int fd, void *buf, size_t len);
extern ssize_t readn_nonblock_timeout(int fd, void *buf, size_t len, struct timeval *timeout);
extern ssize_t writen_nonblock_timeout(int fd, void *buf, size_t len, struct timeval *timeout);

extern int send_fd(int sockfd, int sendfd);
extern int recv_fd(int sockfd, int *recvfd);

extern char *status_str(int status, char *buf, size_t len);

#endif
