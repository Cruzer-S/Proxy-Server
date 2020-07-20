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

void *worker_thread(void *);

void show_address(const char *, struct sockaddr_in *, const char *);

void sig_usr1(int );

Sigfunc *sigset(int , Sigfunc *);

long open_max(void);

int listen_socket(short , int );
int connect_socket(const char *, short );

int nonblocking(int );

int create_epoll_struct(struct epoll_struct *, int );

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in clnt_adr;

	int epoll_fd1, epoll_fd2;
	struct event_struct ev_data;

	struct epoll_struct ep_struct1;
	struct epoll_struct ep_struct2;

	if (argc != 4)
		err_msg("usage: %s <port> <server_ip> <server_port>", ERR_DNG, argv[0]);

	printf("server address: %s:%s \n", argv[2], argv[3]);

	proxy_sock = listen_sock(atoi(argv[1]), BLOG);
	if (proxy_sock < 0)
		err_msg("listen_sock() error: %d", ERR_CTC, proxy_sock);

	if (sigset(SIGUSR1, sig_usr1) == SIG_ERR)
		err_msg("sigset() error", ERR_CTC);

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
			show_address("client address: ", &clnt_adr, " \n");

			if (ret = nonblocking(clnt_sock) != 0) {
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
			if (create_epoll_struct(&ep_struct1, EPOLL_SIZE) != 0)
				err_msg("create_epoll_struct() error!", ERR_CHK);

			if (create_epoll_struct(&ep_struct2, EPOLL_SIZE) != 0)
				err_msg("create_epoll_struct() error!", ERR_CHK);

			if (register_epoll(epoll_fd, clnt_sock) != 0
			||	register_epoll(epoll_fd, serv_sock)) {
				close(clnt_sock);
				close(serv_sock);

				continue;
			}
		}
	}

	close(proxy_sock);

	free(hash_sock);
	free(epoll_event);

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

void show_address(struct sockaddr_in* addr, const char* end_string)
{
	printf("%s%s:%hd%s", 
		//
		//ip address
		inet_ntop(	
			AF_INET, &(addr->sin_addr),
			(char [INET_ADDRSTRLEN]) { /* empty */ },
			INET_ADDRSTRLEN),
		//port number
		ntohs(addr->sin_port),
		//end string
		end_string
	);
}

int listen_socket(short port, int backlog)
{
	int sock;
	struct sockaddr_in sock_adr;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		return -1;

	memset(&sock_adr, 0, sizeof(sock_adr));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_adr.sin_port = htons(port);

	if (bind(sock, (struct sockaddr*) &sock_adr, sizeof(struct sockaddr_in)) == -1) {
		close(sock);
		return -2;
	}

	if (listen(sock, backlog) == -1) {
		close(sock);
		return -3;
	}

	return sock;
}

int connect_socket(const char *ip_addr, short port)
{
	int sock;
	struct sockaddr_in sock_adr;
	
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		return -1;

	memset(&sock_adr, 0, sizeof(struct sockaddr_in));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_addr.s_addr = inet_addr(ip_addr);
	sock_adr.sin_port = htons(port);

	if (connect(sock, (struct sockaddr*)&sock_adr, sizeof(sock_adr)) == -1) {
		close(sock);
		return -2;
	}

	return sock;
}

int nonblocking(int fd)
{
	int flag;

	if (fcntl(fd, F_SETOWN, getpid()) == -1)
		return -1;

	flag = fcntl(fd, F_GETFL, 0);
	if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1)
		return -2;

	return 0;
}

int register_epoll(int epoll_fd, int sock_fd)
{
	struct epoll_event event;

	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1)
		return -1;

	return 0;
}

int create_epoll_struct(struct epoll_struct *ep_struct int* epoll_size)
{
	struct epoll_event *ep_events;
	int epfd;

	epfd = epoll_create(EPOLL_SIZE);
	if (epfd == -1)
		return -1;

	ep_events = malloc(sizeof(struct epoll_event) * epoll_size);
	if (ep_events == NULL)
		return -2;

	ep_struct->epoll_fd = epfd;
	ep_struct->ep_events = ep_events;

	return 0;
}
