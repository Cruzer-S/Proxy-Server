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
#include <netdb.h>

#include "handler/error.h"
#include "handler/socket.h"
#include "handler/atomic.h"
#include "handler/signal.h"

#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)
#define BUFFER_SIZE	(4096)
#define HEADER_SIZE	(4096)		// 4kb
#define MAX_LINE	(1024)

struct event_data {
	int pipe_fd[2];

	char *data;
	int data_size;
};

struct thread_args {
	struct epoll_handler *handler[2];
};

inline int receive_header(int , char *, int);
inline int parse_header(const char *, int , void *);

void *client_thread(void *);
// void *server_thread(void *);

void sig_usr1(int );

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;

	struct event_data *ev_data;

	struct epoll_handler ep_stoc, ep_ctos;
	struct thread_args thd_arg;

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

	if ( (ret = create_epoll_handler(&ep_ctos, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_handler(stoc) error: %d", ERR_CTC, ret);	

	thd_arg.handler[0] = &ep_ctos;

	if (pthread_create((pthread_t [1]) { 0 }, NULL, client_thread, (void *)&thd_arg) != 0)
		err_msg("pthread_create() error", ERR_CTC);

	atomic_print("server setup \n");

	for (;;)
	{
		clnt_sock = accept(
			proxy_sock, 
			(struct sockaddr*)&clnt_adr, 
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
			err_msg("atomic_alloc(ev_data) error", ERR_NRM);
			continue;
		} 
		
		ev_data->pipe_fd[0] = clnt_sock;
		ev_data->pipe_fd[1] = -1;
		ev_data->data = atomic_alloc(HEADER_SIZE);
		ev_data->data_size = HEADER_SIZE;

		if (register_epoll_handler(
				&ep_ctos, clnt_sock, 
				EPOLLIN | EPOLLET, ev_data) != 0) {
			close(clnt_sock);
			atomic_free(ev_data);

			err_msg("register_epoll_handler() error", ERR_NRM);

			continue;
		}
	}

	release_atomic_alloc();

	return 0;
}

void *client_thread(void *args)
{
	struct event_data *ev_data;
	struct thread_args *thd_arg = (struct thread_args *)args;

	struct epoll_handler	*ep_ctos = thd_arg->handler[0],
							*ep_stoc = thd_arg->handler[1];

	int cnt, ret;
	
	for(;;)
	{
		cnt = wait_epoll_handler(ep_ctos);
		if (cnt == -1) {
			err_msg("wait_epoll_handler() error: %d", ERR_CHK, cnt);
			continue;
		}

		for (int i = 0; i < cnt; i++)
		{
			ev_data = get_epoll_handler(ep_ctos);

			if (ev_data == NULL) {
				err_msg("get_epoll_handler() error", ERR_CHK);
				continue;
			}
			
			if (ev_data->pipe_fd[1] == -1) {
				ret = receive_header(ev_data->pipe_fd[0], ev_data->data, ev_data->data_size);
				if (ret < 0) {
					err_msg("receive_header() error", ERR_CHK);
					goto ERROR_HANDLER;
				}

				struct sockaddr_in serv_adr;
				if (parse_header(ev_data->data, 0, &serv_adr) < 0) {
					err_msg("parse_header() error", ERR_CHK);
					goto ERROR_HANDLER;
				}

				ev_data->pipe_fd[1] = connect_socket2(&serv_adr);
				if (ev_data->pipe_fd[1] < 0) {
					err_msg("connect_socket2() error", ERR_CHK);
					goto ERROR_HANDLER;
				}

				show_address(&serv_adr);

				continue; ERROR_HANDLER:
				release_epoll_handler(ep_ctos, ev_data->pipe_fd[0]);
				close(ev_data->pipe_fd[0]);

				atomic_free(ev_data->data);
				atomic_free(ev_data);
			} else {
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

int receive_header(int sock, char *header, int header_size)
{
	int rd_len, str_len;

	rd_len = 0;
	for (;;) {
		str_len = read(sock, &header[rd_len], header_size - rd_len);
		if (str_len == -1) {
			if (errno == EAGAIN)
				return -1;	//It means that
							//1. there's no read data
							//2. however, header isn't complete
			else
				return -2;
		}

		else if (str_len == 0)
			return -3;

		rd_len += str_len;

		if (rd_len == header_size)
			break;

		if (rd_len > 4)
			if ( strstr(&header[rd_len - 5], "\r\n\r\n") != NULL)
				break;
	}

	return 0;
}

int parse_header(const char *header, int type, void *ret)
{
	char line[MAX_LINE];
	switch(type)
	{
	case 0:	; //sockaddr_in
		struct sockaddr_in sock_adr;
		struct hostent *hostent;

		const char *target, *endl;
		char *host, *end;

		target = "Host: ", endl = "\r\n";

		host = strstr(header, target);
		if (host == NULL)	return -1;
		else				host += strlen(target);

		end = strstr(host, endl);
		if (end == NULL)	return -2;
		else				strncpy(line, host, end - host);

		int i = 0;
		memset(&sock_adr, 0, sizeof(struct sockaddr_in));
		if ((i = translate_host(line, &sock_adr)) < 0)
			return -3;

		*(struct sockaddr_in *)ret = sock_adr;
		break;

	case 1:	// connection-type
		break;
	}

	return 1;
}
