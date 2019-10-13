#ifndef GWDUAN_COMMON_H
#define GWDUAN_COMMON_H

extern ssize_t readn(int fd, void *buf, size_t len);
extern ssize_t writen(int fd, void *buf, size_t len);

extern int do_server(int connfd);
extern int do_client(int connfd);

#endif
