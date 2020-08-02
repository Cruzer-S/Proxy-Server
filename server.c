#include <stdio.h>

#include "handler/error.h"
#include "handler/socket.h"

#define BLOG        (15)
#define BUF_SIZE    (1024)

void *worker_thread(void *);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    int recv_len;

    if (argc != 2)
        err_msg("usage: %s <port>", ERR_DNG, argv[0]);

    serv_sock = listen_sock(atoi(argv[1]), BLOG);
    if (serv_sock < 0)
        err_msg("listen_sock() error: %d", ERR_CTC, serv_sock);
    
    for (;;)
    {
        clnt_sock = accept(
			serv_sock, 
			(struct sockaddr*){ /* empty */ }, 
			(int []) { sizeof(clnt_adr) }
		);

        if (clnt_sock == -1) {
            err_msg("accept() error!", ERR_CHK);
            continue;
        }

        if (pthread_reate((int *){ /* empty */ }, NULL, worker_thread, (void *)clnt_sock) != 0) {
            err_msg("pthread_create() error!", ERR_CHK);
            continue;
        }
    }

    close(serv_sock);

    return 0;
}

void *worker_thread(void *args)
{
    char buf[BUF_SIZE];
    int sock = (int)args;
    int recv_len, send_len;

    for (;;)
    {
        recv_len = recv(sock, buf, BUF_SIZE - 1, 0);
        if (recv_len == -1) {
            err_msg("recv() error!", ERR_CHK);
            continue;
        }

        if (recv_len == 0) {
            close(sock);
            break;
        }

        send_len = 0;
        while (send_len < recv_len)
            send_len += send(sock, buf, recv_len, 0);
    }

    close(sock);
}