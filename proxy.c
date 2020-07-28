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

struct thread_args {
	struct epoll_struct ep_struct;
	int buf_size;
};

void *worker_thread(void *);

void sig_usr1(int );

Sigfunc *sigset(int , Sigfunc *);

long open_max(void);

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in clnt_adr;

	int epoll_fd1, epoll_fd2;
	struct event_struct ev_data;

	struct epoll_struct ep_struct_stoc;	//server to client
	struct epoll_struct ep_struct_ctos;	//client to server

	struct thread_args thd_arg;

	pthread_t tid_stoc;
	pthread_t tid_ctos;

	int ret;

	if (argc != 4)
		err_msg("usage: %s <port> <server_ip> <server_port>", ERR_DNG, argv[0]);

	printf("server address: %s:%s \n", argv[2], argv[3]);

	proxy_sock = listen_socket(atoi(argv[1]), BLOG);
	if (proxy_sock < 0)
		err_msg("listen_sock() error: %d", ERR_CTC, proxy_sock);

	if (sigset(SIGUSR1, sig_usr1) == SIG_ERR)
		err_msg("sigset() error", ERR_CTC);

	if ( (ret = create_epoll_struct(&ep_struct_stoc, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_struct(stoc) error: %d", ERR_CTC, ret);
	
	if ( (ret = create_epoll_struct(&ep_struct_ctos, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_struct(ctos) error: %d", ERR_CTC, ret);

	thd_arg.buf_size = BUF_SIZE;

	thd_arg.ep_struct = ep_struct_stoc;
	if (pthread_create(&tid_stoc, NULL, worker_thread, &thd_arg) != 0)
		err_msg("pthread_create(stoc) error", ERR_CTC);

	thd_arg.ep_struct = ep_struct_ctos;
	if (pthread_create(&tid_ctos, NULL, worker_thread, &thd_arg) != 0)
		err_msg("pthread_create(ctos) error", ERR_CTC);

	while (true)
	{
		clnt_sock = accept(
			proxy_sock, (struct sockaddr*)&clnt_adr, 
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
		} else {
			ev_data.pipe_fd[0] = serv_sock;
			ev_data.pipe_fd[1] = clnt_sock;
			if (register_epoll_struct(
				 &ep_struct_stoc, serv_sock, 
				 EPOLLIN | EPOLLET, ev_data, 
				 sizeof(struct event_struct)) != 0)
				goto CLOSE_SOCKET;

			ev_data.pipe_fd[0] = proxy_sock;
			ev_data.pipe_fd[1] = serv_sock;
			if (register_epoll_struct(
				 &ep_struct_ctos, proxy_sock,
				 EPOLLIN | EPOLLET, ev_data,
				 sizeof(struct event_struct)) != 0)
			{
				release_epoll_struct(&ep_struct, serv_sock, ev_data);

				goto CLOSE_SOCKET;
			}

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
	struct thread_args thd_arg = *(struct thread_args*)args;

	struct epoll_event *ep_event = thd_arg.ep_event;
	int epoll_size = thd_arg.epoll_size;
	struct epoll_event event;
	
	struct event_struct ev_struct;

	int event_cnt;

	int buf_size = thd_arg.buf_size;
	char buf[buf_size];
	
	for(;;)
	{
		event_cnt = epoll_wait(epfd, ep_events, epoll_size, -1);
		if (event_cnt == -1) {
			err_msg("epoll_wait() error", ERR_CHK);
			break;
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

long open_max(void)
{
#ifdef OPEN_MAX
long openmax = OPEN_MAX;
#else
#define OPEN_MAX_GUESS 1024
long openmax = 0;
#endif
	if (openmax == 0) {
		errno = 0;
		if ((openmax = sysconf(_SC_OPEN_MAX)) < 0) {
			if (errno == 0)
				openmax = OPEN_MAX_GUESS;
			else
				err_msg("sysconf error for _SC_OPEN_MAX", ERR_CTC);
		}
	}

	return openmax;
}
