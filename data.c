#include <stdio.h>
#include <stdlib.h>
#include "common.h"

int do_server(int connfd)
{
	return do_recv_send(connfd);
}

int do_client(int connfd)
{
	return do_send_recv(connfd);
}
