#include "socket.h"

void show_address(struct sockaddr_in *addr)
{
	printf("type: %s \n", (addr->sin_family == AF_INET) ? "IPv4" : "IPv6");
	printf("address: %s [%hu] \n", 
		//ip address
		inet_ntop(	
			AF_INET, &(addr->sin_addr),
			(char [INET_ADDRSTRLEN]) { /* empty */ },
			INET_ADDRSTRLEN
		), 
		//port number
		ntohs(addr->sin_port)
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
	if (sock == -1)
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

int connect_socket2(struct sockaddr_in *sock_adr)
{
	int sock;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		return -1;

	if (connect(sock, (struct sockaddr*)sock_adr, sizeof(struct sockaddr_in)) == -1) {
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

int translate_host(char *host, struct sockaddr_in *sock_adr)
{
	char *colon;
	struct hostent *hostent;

	sock_adr->sin_family = AF_INET;

	colon = strchr(host, ':');
	if (colon != NULL) {
		char *port;
		uint16_t port_num;

		port = (colon + 1);
		port_num = atoi(port);
		if (port_num == 0) 
			return -1;

		sock_adr->sin_port = htons(port_num);

		*colon = '\0';
		if ( inet_aton(host, &(sock_adr->sin_addr)) ) {
			*colon = ':';
			return 0;
		} else {
			hostent = gethostbyname(host);
			*colon = ':';
			if (hostent == NULL)
				return -2;

			memcpy(&sock_adr->sin_addr, hostent->h_addr_list[0], hostent->h_length);
			return 1;
		}
	} else {
		if ( ! inet_aton(host, &(sock_adr->sin_addr)) ) {
			hostent = gethostbyname(host);
			if (hostent == NULL)
				return -3;

			memcpy(&sock_adr->sin_addr, hostent->h_addr_list[0], hostent->h_length);
			sock_adr->sin_port = htons((uint16_t)80);

			return 3;
		} else {
			sock_adr->sin_port = htons((uint16_t)80);

			return 4;
		}
	}

	return 0;
}

int create_epoll_handler(struct epoll_handler *ep_handler, int epoll_size)
{
	struct epoll_event *ep_events;

	int epfd;
	int err;

	epfd = epoll_create(epoll_size);
	if (epfd == -1)
		return -1;

	ep_events = malloc(sizeof(struct epoll_event) * epoll_size);
	if (ep_events == NULL)
		return -2;

	ep_handler->epoll_fd = epfd;
	ep_handler->ep_events = ep_events;
	ep_handler->epoll_size = epoll_size;

	ep_handler->cur_event = -1;
	ep_handler->get_event = -1;

	return 0;
}

int register_epoll_handler(struct epoll_handler *ep_handler, int sock, int opt, void *data)
{ 
	struct epoll_event event;

	event.events = opt;
	event.data.ptr = data;
	
	if (epoll_ctl(ep_handler->epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1)
		return -1;

	return 0;
}

int wait_epoll_handler(struct epoll_handler *ep_handler)
{	//non-thread-safety
	ep_handler->cur_event = 0;

	return (ep_handler->get_event = epoll_wait(
		ep_handler->epoll_fd, 
		ep_handler->ep_events, 
		ep_handler->epoll_size, -1
	));
}

void *get_epoll_handler(struct epoll_handler *ep_handler)
{	//non-thread-safety
	int cur = ep_handler->cur_event;
	int get = ep_handler->get_event;

	if (get <= 0 || cur >= get)
		return NULL;

	ep_handler->cur_event++;

	return ep_handler->ep_events[cur].data.ptr;
}

int release_epoll_handler(struct epoll_handler *ep_handler, int sock)
{
	if (epoll_ctl(ep_handler->epoll_fd, EPOLL_CTL_DEL, sock, NULL) == -1)
		return -1;

	return 0;
}

void delete_epoll_handler(struct epoll_handler *ep_handler)
{
	close(ep_handler->epoll_fd);
	free(ep_handler->ep_events);
}
