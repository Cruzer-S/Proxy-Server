#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <arpa/inet.h>	//inet_ntop
#include <sys/socket.h>	//socket
#include <sys/epoll.h>	//epoll
#include <stdio.h>		//printf
#include <string.h>		//memset
#include <unistd.h>		//close
#include <fcntl.h>		//fcntl
#include <stdlib.h>		//malloc, free
#include <netdb.h>
#include <stdint.h>

void show_address(struct sockaddr_in *);

int listen_socket(short , int );

int connect_socket(const char *, short );
int connect_socket2(struct sockaddr_in *);

int nonblocking(int );

struct sockaddr_in convert_hostname(const char *);

int translate_host(char *, struct sockaddr_in *);

struct epoll_handler {
	struct epoll_event *ep_events;
	int epoll_fd;
	int epoll_size;

	int get_event;
	int cur_event;
};

int create_epoll_handler(struct epoll_handler *, int );

int register_epoll_handler(struct epoll_handler *, int , int , void *);

int wait_epoll_handler(struct epoll_handler *);

void *get_epoll_handler(struct epoll_handler *);

int release_epoll_handler(struct epoll_handler *, int );

void delete_epoll_handler(struct epoll_handler *);

#endif
