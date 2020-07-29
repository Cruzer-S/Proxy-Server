#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "err_hdl.h"
#include "socket.h"

#include "atomic_style.h"

#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)
#define BUF_SIZE	(1024)

typedef void Sigfunc(int);

struct event_struct {
	int pipe_fd[2];
};

void *worker_thread(void *);

void sig_usr1(int );

Sigfunc *sigset(int , Sigfunc *);

long open_max(void);

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in clnt_adr;

	struct epoll_handler ep_stoc, ep_ctos;	//server to client, client to server

	int ret;

	if (argc != 4) {
		err_msg("usage: %s <proxy_port> <server_ip> <server_port>", ERR_DNG, argv[0]);
	} else {
		printf("proxy port: %s \n", argv[1]);
		printf("server address: %s:%s \n", argv[2], argv[3]);

		if (sigset(SIGUSR1, sig_usr1) == SIG_ERR)
			err_msg("sigset() error", ERR_CTC);
	}

	proxy_sock = listen_socket(atoi(argv[1]), BLOG);
	if (proxy_sock < 0)
		err_msg("listen_sock() error: %d", ERR_CTC, proxy_sock);

	if ( (ret = create_epoll_handler(&ep_stoc, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_struct(stoc) error: %d", ERR_CTC, ret);
	
	if ( (ret = create_epoll_handler(&ep_ctos, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_struct(ctos) error: %d", ERR_CTC, ret);

	if ((pthread_create(&tid_stoc, NULL, worker_thread, (void *)epoll_handler) != 0)
	||	(pthread_create(&tid_ctos, NULL, worker_thread, (void *)epoll_handler) != 0))
		err_msg("pthread_create() error", ERR_CTC);

	while (true)
	{
		clnt_sock = accept(
			proxy_sock, 
			(struct sockaddr*)&clnt_adr, 
			(int []) { sizeof(clnt_adr) }
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

		if( (serv_sock = connect_socket(argv[2], atoi(argv[3]))) < 0) {
			err_msg("connect_socket() error: %d", ERR_CHK, serv_sock);
			close(clnt_sock);
			continue;

		}
		
		if (register_epoll_struct(&ep_struct_stoc, serv_sock) != 0)
			goto CLOSE_SOCKET;

		if (register_epoll_struct(&ep_struct_ctos, proxy_sock) != 0)
			goto CLOSE_SOCKET;

CLOSE_SOCKET:
			close(clnt_sock);
			close(serv_sock);
		}

		show_address("client address: ", &clnt_adr, " \n");
		printf("connect client successfully \n");
	}

	close(proxy_sock);

	return 0;
}

void *worker_thread(void *args)
{
	struct epoll_handler *handler = (struct epoll_handler*)args;

	int cnt;
	
	for(;;)
	{
		cnt = wait_epoll_handler(handler);
		if (cnt == -1) {
			err_msg("wait_epoll_handler() error: %d", ERR_CHK, cnt);
			continue;
		}

		for (int i = 0; i < event_cnt; i++)
		{
			ev_struct = *(struct event_struct)ep_events[i].data.ptr;
			str_len = read(ev_struct.pipe_fd[0], buf, buf_size);
			if (str_len == -1)
				err_msg("read() error", ERR_CHK);

			if (str_len == 0) {
				epoll_ctl(epfd, EPOLL_CTL_DEL, ev_struct.pipe_fd[0], NULL);
				close(ev_struct.pipe_fd[0]);

				free(ev_struct);

				printf("closed client: %d \n", ep_events[i].data.fd);
			} else {
				if (write(ev_struct.pipe_fd[1], buf, str_len) == -1)
					err_msg("write() error", ERR_CHK);
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

Sigfunc *sigset(int signo, Sigfunc *func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
	} else {
		act.sa_flags |= SA_RESTART;
	}

	if (sigaction(signo, &act, &oact) < 0)
		return SIG_ERR;

	return (oact.sa_handler);
}