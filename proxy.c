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

struct thread_arg {
	struct epoll_event *ep_events;
	int epoll_fd;

	int *hash_sock;
};

void *worker_thread(void *);

void show_address(struct sockaddr_in* , const char *);

void sig_usr1(int );

Sigfunc *sigset(int , Sigfunc *);

long open_max(void);

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in proxy_adr, serv_adr, clnt_adr;

	struct epoll_event *ep_events;
	struct epoll_event event;
	int epoll_fd;

	int flag;

	pthread_t worker_tid;
	struct thread_arg thd_arg;

	int *hash_sock;

	if (argc != 4)
		err_msg("usage: %s <port> <server_ip> <server_port>", ERR_DNG, argv[0]);

	printf("server address: %s:%s \n", argv[2], argv[3]);

	printf("open_max: %ld \n", open_max()); 
	hash_sock = malloc(sizeof(int) * open_max());
	if (hash_sock == NULL)
		err_msg("malloc() error", ERR_CTC);

	proxy_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (proxy_sock == -1)
		err_msg("socket(proxy) error", ERR_CTC);

	memset(&proxy_adr, 0, sizeof(proxy_adr));
	proxy_adr.sin_family = AF_INET;
	proxy_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_adr.sin_port = htons(atoi(argv[1]));

	if (bind(proxy_sock, (struct sockaddr*) &proxy_adr, sizeof(proxy_adr)) == -1)
		err_msg("bind() error", ERR_CTC);

	if (listen(proxy_sock, BLOG) == -1)
		err_msg("lisetn() error", ERR_CTC);

	epoll_fd = epoll_create(EPOLL_SIZE);
	if (epoll_fd == -1)	
		err_msg("epoll_cretae() error", ERR_DNG);

	ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
	if (ep_events == NULL)
		err_msg("malloc() error", ERR_DNG);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[2]);
	serv_adr.sin_port = htons(atoi(argv[3]));

	thd_arg = (struct thread_arg) {
		.ep_events = ep_events,
		.epoll_fd = epoll_fd,
		.hash_sock = hash_sock
	};

	if (pthread_create(
			&worker_tid, NULL, 
			worker_thread, (void*)&thd_arg) != 0)	
		err_msg("pthread_create() error", ERR_CTC);

	if (sigset(SIGUSR1, sig_usr1) == SIG_ERR)
		err_msg("sigset() error", ERR_CTC);

	while (true)
	{
		clnt_sock = accept(
			proxy_sock, (struct sockaddr*)&clnt_adr, (int []) { sizeof(clnt_adr) }
		);

		if (clnt_sock == -1) {
			err_msg("accept() error", ERR_CHK);
			continue;
		}

		serv_sock = socket(PF_INET, SOCK_STREAM, 0);
		if (serv_sock == -1) {
			err_msg("socket(server) error", ERR_CHK);

			continue;
		}

		if (connect(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
			err_msg("connect() error", ERR_CHK);
			close(clnt_sock);
			continue;
		}

		if (fcntl(clnt_sock, F_SETOWN, getpid()) == -1) {
			err_msg("fcntl(F_SETOWN) error", ERR_CHK);
			goto ERROR;
		}

		flag = fcntl(clnt_sock, F_GETFL, 0);
		if (fcntl(clnt_sock, F_SETFL, flag | O_NONBLOCK) == -1) {
			err_msg("fcntl(F_SETFL) error", ERR_CHK);
			goto ERROR;
		}

		event.events = EPOLLIN | EPOLLET;
		event.data.fd = clnt_sock;
				
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clnt_sock, &event) == -1) {
			err_msg("epoll_ctl(client) error", ERR_CHK);
			goto ERROR;
		}

		printf("client address: ");
		show_address(&clnt_adr, " \n");

		hash_sock[clnt_sock] = serv_sock;
		printf("connected client: %d \n", clnt_sock);

		continue; ERROR:
		close(clnt_sock);
		close(serv_sock);
		hash_sock[clnt_sock] = -1;
	}

	close(proxy_sock);

	free(hash_sock);
	free(epoll_event);

	return 0;
}

void *worker_thread(void *args)
{
	struct thread_arg thd_arg = *(struct thread_arg *)args;

	struct epoll_event event, *ep_events;
	int serv_fd, clnt_fd, epoll_fd;
	int *hash_sock;

	ep_events = thd_arg.ep_events;
	epoll_fd = thd_arg.epoll_fd;
	hash_sock = thd_arg.hash_sock;

	while (1)
	{
		event_cnt = epoll_wait(epoll_fd, ep_events, EPOLL_SIZE, -1);
		if (event_cnt == -1)
			err_msg("epoll_wait() error", ERR_CHK);

		for (int i = 0; i < event_cnt; i++)
		{
			clnt_fd = ep_events[i].data.fd;
			if ( (serv_fd = hash_sock[clnt_fd]) == -1) {
				err_msg("corresponding serv_fd isn't register", ERR_NRM);
				continue;
			}

			str_len = read(clnt_fd, buf, BUF_SIZE);
			if (str_len == 0) {
				epoll_ctl(
					epoll_fd, EPOLL_CTL_DEL, clnt_fd, NULL
				);

				hash_sock[clnt_fd] = -1;
				close(clnt_fd);

				printf("closed client: %d \n", clnt_fd);

				continue;
			}

			write(serv_fd, buf, str_len);
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

void show_address(struct sockaddr_in* addr, const char* end_string)
{
	printf("%s:%hd%s", 
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


