#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "handler/error.h"
#include "handler/socket.h"
#include "handler/atomic.h"
#include "handler/signal.h"

#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)
#define BUFFER_SIZE	(4096)
#define HEADER_SIZE	(4096)		// 4kb

struct event_data {
	int pipe_fd[2];
	int read_len;
	char *header;
	bool is_client;
};

int receive_header(struct event_data );

void *worker_thread(void *);

void sig_usr1(int );

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;

	struct event_data *ev_data[2];

	struct epoll_handler ep_stoc, ep_ctos;

	int ret;

	if (argc != 2) {
		err_msg("usage: %s <proxy_port>", ERR_DNG, argv[0]);
	} else {
		if (sigset(SIGUSR1, sig_usr1) == SIG_ERR)
			err_msg("sigset() error", ERR_CTC);

		if (init_atomic_alloc() == -1)
			err_msg("init_atomic_alloc() error", ERR_CTC);
	}

	proxy_sock = listen_socket(atoi(argv[1]), BLOG);
	if (proxy_sock < 0)
		err_msg("listen_sock() error: %d", ERR_CTC, proxy_sock);

	if ( (ret = create_epoll_handler(&ep_stoc, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_handler(stoc) error: %d", ERR_CTC, ret);
	
	if ( (ret = create_epoll_handler(&ep_ctos, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_handler(ctos) error: %d", ERR_CTC, ret);

	if (pthread_create((pthread_t [1]) { 0 }, NULL, worker_thread, (void *)&ep_stoc) != 0)
		err_msg("pthread_create() error", ERR_CTC);
	
	if (pthread_create((pthread_t [1]) { 0 }, NULL, worker_thread, (void *)&ep_ctos) != 0)
		err_msg("pthread_create() error", ERR_CTC);

	for (;;)
	{
		clnt_sock = accept(
			proxy_sock, 
			(struct sockaddr*){ 0 }, 
			(int []) { sizeof(struct sockaddr_in) }
		);

		if (clnt_sock == -1) {
			err_msg("accept() error", ERR_CHK);
			continue;
		} else { 
			if ( (ret = nonblocking(clnt_sock)) != 0) {
				close(clnt_sock);
				err_msg("nonblocking() error: %d ", ERR_CHK, ret);

				continue;
			}
		}

		ev_data = atomic_alloc(sizeof(struct event_data));
		if (ev_data == NULL) { 
			err_msg("atomic_alloc(ev_data) error", ERR_CHK);
			continue;
		} else {
			ev_data->pipe_fd[0] = clnt_sock;
			ev_data->read_len = 0;
			ev_data->is_client = true;

			ev_data->header = atomic_alloc(HEADER_SIZE);
			if (ev_data->header == NULL) {
				err_msg("atomic_alloc(header) error", ERR_CHK);
				continue;
			}
		}

		if (register_epoll_handler(
				&ep_ctos, clnt_sock, 
				EPOLLIN, ev_data) != 0) {
			close(clnt_sock);
			atomic_free(ev_data);

			err_msg("register_epoll_handler() error", ERR_CHK)
		}
	}

	release_atomic_alloc();

	return 0;
}

void *worker_thread(void *args)
{
	struct epoll_handler *handler = (struct epoll_handler *)args;
	struct event_data *ev_data;
	int str_len, cnt;

	int buf[BUFFER_SIZE];
	
	for(;;)
	{
		cnt = wait_epoll_handler(handler);
		if (cnt == -1) {
			err_msg("wait_epoll_handler() error: %d", ERR_CHK, cnt);
			continue;
		}

		for (int i = 0; i < cnt; i++)
		{
			ev_data = get_epoll_handler(handler);

			if (ev_data == NULL) {
				err_msg("get_epoll_handler() error", ERR_CHK);
				continue;
			}

			if (ev_data->is_client) {

				/*
				int rd_len = ev_data->read_len;

				// read header
				str_len = read(
					ev_data->pipe_fd[0],
					&ev_data->header[rd_len],
					HEADER_SIZE - rd_len
				);

				if (str_len == -1) {
					err_msg("read() error", ERR_CHK);
				}

				if (str_len == 0) {
					release_epoll_handler(handler, ev_data->pipe_fd[0]);
					close(ev_data->pipe_fd[0]);
					atomic_free(ev_data->header);
				} else {
				}
				*/
			} else {	// server
				str_len = read(ev_data->pipe_fd[0], buf, BUFFER_SIZE);
				if (str_len == -1) {
					err_msg("read() error", ERR_CHK);
					continue;
				}

				if (str_len == 0) {
					release_epoll_handler(handler, ev_data->pipe_fd[0]);

					close(ev_data->pipe_fd[0]);
					atomic_free(ev_data);
				} else {
					write(ev_data->pipe_fd[1], buf, str_len);
				}
			}
		}
	}

	return (void *)0;
}

void sig_usr1(int signo)
{
	write(STDOUT_FILENO, "sigusr1", 7);
	exit(EXIT_SUCCESS);
}
