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
#define BUF_SIZE	(1024)

void *worker_thread(void *);

void sig_usr1(int );

struct event_data {
	int pipe_fd[2];
};

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

		serv_adr = ParseHeader();
		/*
		if( (serv_sock = connect_socket(argv[2], atoi(argv[3]))) < 0) {
			err_msg("connect_socket() error: %d", ERR_CHK, serv_sock);
			close(clnt_sock);

			continue;
		}
		*/

		ev_data[0] = atomic_alloc(sizeof(struct event_data));
		ev_data[1] = atomic_alloc(sizeof(struct event_data));
		if (ev_data[0] == NULL || ev_data[1] == NULL) { 
			err_msg("malloc(event_data) error", ERR_CHK);
			continue;
		}

		ev_data[0]->pipe_fd[0] = serv_sock;
		ev_data[0]->pipe_fd[1] = clnt_sock;
		if (register_epoll_handler(
				&ep_stoc, serv_sock, 
				EPOLLIN, ev_data[0]) != 0) {

			goto FAILED_TO_REGISTER;
		}

		ev_data[1]->pipe_fd[0] = clnt_sock;
		ev_data[1]->pipe_fd[1] = serv_sock;
		if (register_epoll_handler(
				&ep_ctos, clnt_sock, 
				EPOLLIN, ev_data[1]) != 0) {
			
			release_epoll_handler(&ep_stoc, serv_sock);

			goto FAILED_TO_REGISTER;
		}

		// show_address("client address: ", &clnt_adr, " \n");
		// printf("connect client successfully \n");
		// printf("proxy: %d \nclient: %d \n server: %d \n"
		// 	,proxy_sock, clnt_sock, serv_sock
		// );

		continue; 
		
		FAILED_TO_REGISTER:
		close(clnt_sock);
		close(serv_sock);

		atomic_free(ev_data[0]);
		atomic_free(ev_data[1]);

		err_msg("register_epoll_handler() error", ERR_CHK);
	}

	close(proxy_sock);
	release_atomic_alloc();

	return 0;
}

void *worker_thread(void *args)
{
	struct epoll_handler *handler = (struct epoll_handler *)args;
	struct event_data *ev_data;
	int str_len, cnt;

	int buf[BUF_SIZE];
	
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

			str_len = read(ev_data->pipe_fd[0], buf, BUF_SIZE);
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

	return (void *)0;
}

void sig_usr1(int signo)
{
	write(STDOUT_FILENO, "sigusr1", 7);
	exit(EXIT_SUCCESS);
}
