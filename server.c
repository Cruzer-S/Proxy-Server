#include <stdio.h>
#include <pthread.h>

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

    serv_sock = listen_socket(atoi(argv[1]), BLOG);
    if (serv_sock < 0)
        err_msg("listen_sock() error: %d", ERR_CTC, serv_sock);
    
    for (;;)
    {
        clnt_sock = accept(
			serv_sock, 
			(struct sockaddr*){ 0 }, 
			(int []) { sizeof(struct sockaddr_in) }
		);

		// printf("client access \n");

        if (clnt_sock == -1) {
            err_msg("accept() error!", ERR_CHK);
            continue;
        }

		// printf("create thread \n");
        if (pthread_create((pthread_t [1]){ 0, }, NULL, worker_thread, (void *)&clnt_sock) != 0) {
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
    int sock = *(int*)args;
    int wr_len, rd_len, count;

    for (;;)
    {
        rd_len = read(sock, buf, BUF_SIZE);
        if (rd_len == -1) {
            err_msg("read() error!", ERR_CHK);
            continue;
        }

        if (rd_len == 0) {
            close(sock);
            break;
        }

		wr_len = 0;
        while (wr_len < rd_len) {
            count = write(sock, &buf[wr_len], rd_len - wr_len);

			if (count == -1) {
				err_msg("write() error", ERR_CHK);
				break;
			}
			wr_len += count;
		}
    }

    close(sock);
}
