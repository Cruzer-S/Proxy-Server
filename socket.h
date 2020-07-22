#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

void show_address(const char *, struct sockaddr_in *, const char *);

int listen_socket(short , int );
int connect_socket(const char *, short );

int nonblocking(int );

int create_epoll_struct(struct epoll_struct *, size_t );

int register_epoll_struct(struct epoll_struct * ,int ,int , size_t );

void release_epoll_struct(struct epoll_struct *);

#endif