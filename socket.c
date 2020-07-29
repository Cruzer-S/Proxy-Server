#define "socket.h"

void show_address(const char *start_string, struct sockaddr_in* addr, const char *end_string)
{
	printf("%s%s:%hd%s", 
		//start string
		start_string,
		//ip address
		inet_ntop(	
			AF_INET, &(addr->sin_addr),
			(char [INET_ADDRSTRLEN]) { /* empty */ },
			INET_ADDRSTRLEN
		), 
		//port number
		ntohs(addr->sin_port),
		//end string
		end_string
	);
}

int accept_socket(int sock, struct sockaddr* sock_adr, int *adr_size)
{
	struct sockaddr_in sock
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
{
	ep_handler->cur_event = 0;

	return (ep_handler->get_event = epoll_wait(
		ep_handler->epoll_fd, 
		ep_handler->ep_events, 
		ep_handler->ep_size, -1
	));
}

void *get_epoll_handler(struct epoll_handler *ep_handler)
{
	int cur = ep_handler->cur_event;
	int get = ep_handler->get_event;

	if (cur <= 0 || get <= 0 || cur >= get)
		return NULL;

	return ep_handler->ep_events[ep_handler->cur_event++];
}

int release_epoll_handler(struct epoll_handler *ep_handler, int sock)
{
	if (epoll_ctl(ep_handler->epoll_fd, EPOLL_CTL_DEL, sock, NULL) == -1)
		return -1;

	return 0;
}

void delete_epoll_handler(struct epoll_struct *ep_handler)
{
	close(ep_handler->epoll_fd);
	free(ep_handler->ep_events);
}