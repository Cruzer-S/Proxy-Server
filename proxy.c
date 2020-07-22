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

#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)

typedef void Sigfunc(int);

struct epoll_struct {
	struct epoll_event *ep_events;
	int epoll_fd;
};

struct event_struct {
	int pipe_fd[2];
};

struct thread_args {
	struct epoll_event *ep_struct;
	struct event_struct *ev_struct;
	int proxy_sock;
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

	int ret;

	pthread_t tid1, tid2;

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

	if (pthread_create(&tid1, NULL, worker_thread, ))		

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
			if (register_epoll_struct(&ep_struct_stoc, serv_sock, EPOLLIN | EPOLLET) != 0
			||	register_epoll_struct(&ep_struct_ctos, clnt_sock, EPOLLIN | EPOLLET) != 0) {
				close(clnt_sock);
				close(serv_sock);

				continue;
			}
		}

		show_address("client address: ", &clnt_adr, " \n");
		printf("connect client successfully \n");
	}

	close(proxy_sock);

	return 0;
}

void *worker_thread(void *args)
{
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