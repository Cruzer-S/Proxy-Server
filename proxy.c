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
	struct event_data *target;
};

struct thread_args {
	int buffer_size, header_size;
	int type;
	struct epoll_handler *handler[2];
};

inline int receive_header(int , char *, int);
inline int parse_header(const char *, int , void *);
inline int connect_server(struct event_data *, char *, int );
inline struct event_data *create_event_data(int , int , struct event_data *);

void *worker_thread(void *);

int main(int argc, char *argv[])
{
	int proxy_sock, serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;

	struct event_data *ev_data;

	struct epoll_handler ep_stoc, ep_ctos;
	struct thread_args serv_arg, clnt_arg;

	int ret;

	if (argc != 2) {
		err_msg("usage: %s <proxy_port>", ERR_DNG, argv[0]);
	} else {
		if (init_atomic_alloc() == -1)
			err_msg("init_atomic_alloc() error", ERR_CTC);
	}

	proxy_sock = listen_socket(atoi(argv[1]), BLOG);
	if (proxy_sock < 0)
		err_msg("listen_sock() error: %d", ERR_CTC, proxy_sock);

	if ( (ret = create_epoll_handler(&ep_ctos, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_handler(ctos) error: %d", ERR_CTC, ret);

	if ( (ret = create_epoll_handler(&ep_stoc, EPOLL_SIZE)) != 0)
		err_msg("create_epoll_handler(stoc) error: %d", ERR_CTC, ret);

	serv_arg.type = 0;
	serv_arg.header_size = 0;
	serv_arg.buffer_size = BUFFER_SIZE;
	serv_arg.handler[0] = &ep_stoc;
	serv_arg.handler[1] = &ep_ctos;
	if (pthread_create((pthread_t [1]) { 0 }, NULL, worker_thread, (void *)&serv_arg) != 0)
		err_msg("pthread_create() error", ERR_CTC);

	clnt_arg.type = 1;
	clnt_arg.header_size = HEADER_SIZE;
	clnt_arg.buffer_size = BUFFER_SIZE;
	clnt_arg.handler[0] = &ep_ctos;
	clnt_arg.handler[1] = &ep_stoc;
	if (pthread_create((pthread_t [1]) { 0 }, NULL, worker_thread, (void *)&clnt_arg) != 0)
		err_msg("pthread_create() error", ERR_CTC);

	printf("start proxy server [%hu] \n", atoi(argv[1]));
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

		ev_data = create_event_data(clnt_sock, -1, NULL);
		if (ev_data == NULL) { 
			close(clnt_sock);
			err_msg("create_event_data() error", ERR_NRM);
			continue;
		}
	
		if (register_epoll_handler(
				&ep_ctos, clnt_sock, 
				EPOLLIN | EPOLLET, ev_data) != 0) {
			close(clnt_sock);
			atomic_free(ev_data);
			
			err_msg("register_epoll_handler() error", ERR_NRM);
			continue;
		}

		printf("connect client: %d \n", clnt_sock);
	}

	release_atomic_alloc();

	return 0;
}

void *worker_thread(void *args)
{
	struct event_data *ev_data;
	struct thread_args *thd_arg = (struct thread_args *)args;

	struct epoll_handler	*handler		= thd_arg->handler[0],
							*target_handler = thd_arg->handler[1];

	int buffer_size = thd_arg->buffer_size;
	int header_size = thd_arg->header_size;

	int count;
	
	for(;;)
	{
		count = wait_epoll_handler(handler);
		if (count == -1) {
			err_msg("wait_epoll_handler() error: %d", ERR_CHK, count);
			continue;
		}

		for (int i = 0; i < count; i++)
		{
			ev_data = get_epoll_handler(handler);

			if (ev_data == NULL) {
				err_msg("get_epoll_handler() error", ERR_CHK);
				continue;
			}
			
			if (ev_data->pipe_fd[1] == -1) 
			{
				int read_len;
				char header[header_size];
				if ( (read_len = connect_server(ev_data, header, header_size)) < 0) {
					err_msg("connect_server() error", ERR_CHK);
					goto CLEANUP;
				}

				struct event_data *serv_data = 
					create_event_data(ev_data->pipe_fd[1], ev_data->pipe_fd[0], ev_data);

				serv_data->target = ev_data;
				if (serv_data == NULL) {
					err_msg("create_event_data() error", ERR_CHK);
					goto CLEANUP;
				}
					
				if (register_epoll_handler(
						target_handler, serv_data->pipe_fd[0], 
						EPOLLIN | EPOLLET, serv_data) != 0) {
					err_msg("register_epoll_handler() error", ERR_NRM);

					atomic_free(serv_data);
					close(serv_data->pipe_fd[0]);

					goto CLEANUP;
				}

				ev_data->target = serv_data;

				write(ev_data->pipe_fd[1], header, read_len);

				printf("establish connection [%d <-> %d] \n", 
							ev_data->pipe_fd[0], ev_data->pipe_fd[1]);
			} 
			else 
			{
				char buffer[buffer_size];

				for (;;) 
				{
					int str_len = read(ev_data->pipe_fd[0], buffer, buffer_size);
					if (str_len == 0) {
						printf("disconnect signal [%d -> %d] \n", ev_data->pipe_fd[0], ev_data->pipe_fd[1]);

						release_epoll_handler(target_handler, ev_data->pipe_fd[1]);
						close(ev_data->pipe_fd[1]);
						atomic_free(ev_data->target);

						goto CLEANUP;
					} else if (str_len < 0) {
						if (errno == EAGAIN) {
							break;
						} else {
							err_msg("read() error", ERR_CHK);
							
							release_epoll_handler(target_handler, ev_data->pipe_fd[1]);
							close(ev_data->pipe_fd[1]);
							atomic_free(ev_data->target);

							goto CLEANUP;
						}
					} else {
						printf("send data [%d -> %d] \n", ev_data->pipe_fd[0], ev_data->pipe_fd[1]);
						write(ev_data->pipe_fd[1], buffer, str_len);
					}
				}
			}

			continue;

			CLEANUP: ; {
				release_epoll_handler(handler, ev_data->pipe_fd[0]);
				close(ev_data->pipe_fd[0]);
				atomic_free(ev_data);
			}
		}
	}

	return (void *)0;
}

int receive_header(int sock, char *header, int header_size)
{
	int rd_len, str_len;

	rd_len = 0;
	for (;;) 
	{
		str_len = read(sock, &header[rd_len], header_size - rd_len);
		if (str_len == -1) {
			if (errno == EAGAIN)
				return -1;	
			else
				return -2;
		} 
		
		if (str_len == 0) return -3;

		rd_len += str_len;

		if (rd_len == header_size) {
			header[header_size - 1] = '\0';
			break;
		}

		header[rd_len] = '\0';
		if (rd_len > 4)
			if ( strstr(&header[rd_len - 5], "\r\n\r\n") != NULL)
				break;
	}

	return rd_len;
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
		line[end - host] = 0;
		
		memset(&sock_adr, 0, sizeof(struct sockaddr_in));
		if (translate_host(line, &sock_adr) < 0) {
			printf("failed to translate_host() \n");
			return -3;
		}

		*(struct sockaddr_in *)ret = sock_adr;
		break;

	case 1:	// Method

		break;
	}

	return 1;
}

int connect_server(struct event_data *ev_data, char *header, int header_size)
{
	int read_len;
	struct sockaddr_in serv_adr;

	if ( (read_len = receive_header(ev_data->pipe_fd[0], header, header_size)) < 0) {
		printf("failed to receive_header() \n");
		return -1;
	}

	if (parse_header(header, 0, &serv_adr) < 0) {
		printf("failed to parse_header() \n");
		return -2;
	}

	if ( (ev_data->pipe_fd[1] = connect_socket2(&serv_adr, 0, 1)) < 0) {
		printf("failed to connect_socket2() \n");
		return -3;
	}

	return read_len;
}

struct event_data *create_event_data(int sock1, int sock2, struct event_data *target)
{
	struct event_data *ev_data = atomic_alloc(sizeof(struct event_data));

	if (ev_data == NULL)
		return NULL;

	ev_data->pipe_fd[0] = sock1;
	ev_data->pipe_fd[1] = sock2;
	ev_data->target = target;

	return ev_data;
}

