#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "err_hdl.h"

#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)

#define HEADER_SIZE	(1024 * 8)	//8Kb

struct thread_arg {
	struct sockaddr_in serv_adr;
	int clnt_sock;
};

void *worker_thread(void *);

void show_address(struct sockaddr_in* , const char *);

int main(int argc, char *argv[])
{
	int proxy_sock, clnt_sock;
	struct sockaddr_in proxy_adr, clnt_adr;

	struct epoll_event *ep_events;
	struct epoll_event event;
	int epfd;

	int flag;

	struct thread_arg thd_arg;

	if (argc != 4)
		err_msg("usage: %s <port> <server_ip> <server_port>", ERR_DNG, argv[0]);

	printf("server address: %s:%s \n", argv[2], argv[3]);

	proxy_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (proxy_sock == -1)
		err_msg("socket() error", ERR_CTC);

	memset(&proxy_adr, 0, sizeof(proxy_adr));
	proxy_adr.sin_family = AF_INET;
	proxy_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_adr.sin_port = htons(atoi(argv[1]));

	if (bind(proxy_sock, (struct sockaddr*) &proxy_adr, sizeof(proxy_adr)) == -1)
		err_msg("bind() error", ERR_CTC);

	if (listen(proxy_sock, BLOG) == -1)
		err_msg("lisetn() error", ERR_CTC);

	epfd = epoll_create(EPOLL_SIZE);
	if (epfd == -1)
		err_msg("epoll_cretae() error", ERR_DNG);

	ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
	if (ep_events == NULL)
		err_msg("malloc() error", ERR_DNG);

	while (true)
	{
		clnt_sock = accept(
			proxy_sock, (struct sockaddr*)&clnt_adr, (int []) { sizeof(clnt_adr) }
		);

		printf("client address: ");
		show_address(&clnt_adr, " \n");

		if (clnt_sock == -1) {
			err_msg("accept() error", ERR_CHK);

			continue;
		}

	 	if (fcntl(clnt_sock, F_SETOWN, getpid()) == -1) {
			err_msg("fcntl(F_SETOWN) error", ERR_CHK);
			close(clnt_sock);

			continue;
		}

		flag = fcntl(clnt_sock, F_GETFL, 0);
		if (fcntl(clnt_sock, F_SETFL, flag | O_NONBLOCK) == -1) {
			err_msg("fcntl(F_SETFL) error", ERR_CHK);
			close(clnt_sock);

			continue;
		}

		event.events = EPOLLIN | EPOLLET;
		event.data.fd = clnt_sock;
				
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event) == -1) {
			err_msg("epoll_ctl(client) error", ERR_CHK);
			close(clnt_sock);

			continue;
		}

		printf("connected client: %d \n", clnt_sock);

		thd_arg = (struct thread_arg) {
			.serv_adr = (struct sockaddr_in) {
				.sin_family = AF_INET,
				.sin_addr.s_addr = inet_addr(argv[2]),
				.sin_port = htons(atoi(argv[3]))
			},	//Do not use serv_adr to socket function directly,
				//remained member(or byte) isn't filled with zero
			clnt_sock = clnt_sock
		};
	}

	close(proxy_sock);

	return 0;
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

void *worker_thread(void *args)
{
	struct thread_arg thd_arg = *(struct thread_arg *)args;



	return (void *)0;
}
