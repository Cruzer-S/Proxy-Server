#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "err_hdl.h"

#define BUF_SIZE	(4096)
#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)

int main(int argc, char *argv[])
{
	int proxy_sock;
	struct sockaddr_in proxy_adr;
	char buf[BUF_SIZE];

	if (argc != 4)
		err_msg("usage: %s <port> <server_ip> <server_port>", ERR_DNG, argv[0]);

	printf("server address: %s:%s \n", argv[2], argv[3]);

	proxy_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (proxy_sock == -1)
		err_msg("socket(2) error", ERR_CTC);

	memset(&proxy_adr, 0, sizeof(proxy_adr));
	proxy_adr.sin_family = AF_INET;
	proxy_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_adr.sin_port = htons(atoi(argv[1]));

	if (bind(proxy_sock, (struct sockaddr*) &proxy_adr, sizeof(proxy_adr)) == -1)
		err_msg("bind(2) error", ERR_CTC);

	if (listen(proxy_sock, BLOG) == -1)
		err_msg("lisetn(2) error", ERR_CTC);

	struct epoll_event *ep_events;
	struct epoll_event event;
	int epfd, event_cnt;

	epfd = epoll_create(EPOLL_SIZE);
	ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

	event.events = EPOLLIN;
	event.data.fd = proxy_sock;
	epoll_ctl(epfd, EPOLL_CTL_ADD, proxy_sock, &event);

	while (1)
	{
		event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
		if (event_cnt == -1)
		{
			err_msg("epoll_wait(2) error", ERR_CHK);
			break;
		}

		for (int i = 0; i < event_cnt; i++)
		{
			printf("event.data.ptr: %p \n", ep_events[i].data.ptr);

			if (ep_events[i].data.fd == proxy_sock)
			{
				struct sockaddr_in clnt_adr;

				int adr_sz = sizeof(clnt_adr);
				int clnt_sock = accept(proxy_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
				event.events = EPOLLIN | EPOLLET;
				event.data.fd = clnt_sock;

				epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
			} else {
				int str_len;

				while (true)
				{
					str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);
					if (str_len == 0)	//close-request
					{
						epoll_ctl(
							epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL
						);
						close(ep_events[i].data.fd);
						printf("closed client: %d \n", ep_events[i].data.fd);
						break;
					} else if (str_len < 0) {
						if (errno == EAGAIN)
							break;
					} else {
						write(ep_events[i].data.fd, buf, str_len);
					}
				}
			}
		}
	}

	free(ep_events); 
	close(proxy_sock);
	close(epfd);

	return 0;
}
