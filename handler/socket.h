#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

void show_address(const char *, struct sockaddr_in *, const char *);

int listen_socket(short , int );

int connect_socket(const char *, short );

int nonblocking(int );


struct epoll_handler {
	struct epoll_event *ep_events;
	int epoll_fd;
	int epoll_size;

	int get_event;
	int cur_event;
};

int create_epoll_handler(struct epoll_handler *, int )

int register_epoll_handler(struct epoll_handler *, int , int , void *)

int wait_epoll_handler(struct epoll_handler *);

void *get_epoll_handler(struct epoll_handler *);

int release_epoll_handler(struct epoll_handler *, int )

void delete_epoll_handler(struct epoll_struct *)

#endif
