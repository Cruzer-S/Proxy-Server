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
#include "queue.h"

#define BLOG		(10)		//back-log
#define EPOLL_SIZE	(50)

#define HEADER_SIZE	(1024 * 8)	//8Kb

struct queue_data {
	int fd;
	char data[HEADER_SIZE];
};

struct thread_arg {
	int proxy_sock;
	struct sockaddr_in serv_adr;
	struct queue* q;
	int epfd;
};

void *worker_thread(void* );

long get_processor(void);

int main(int argc, char *argv[])
{
	int proxy_sock;
	struct sockaddr_in proxy_adr;
	struct queue header_queue;

	struct epoll_event *ep_events;
	struct epoll_event event;
	int epfd, event_cnt;

	long nprocs;
	struct thread_arg thd_arg;
	void *ret;

	if (argc != 4)
		err_msg("usage: %s <port> <server_ip> <server_port>", ERR_DNG, argv[0]);

	printf("server address: %s:%s \n", argv[2], argv[3]);

	if (queue_create(&header_queue, sizeof(struct queue_data), EPOLL_SIZE) < 0)
		err_msg("queue_create() error", ERR_DNG);

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

	event.events = EPOLLIN;
	event.data.fd = proxy_sock;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, proxy_sock, &event) == -1)
		err_msg("epoll_ctl() error", ERR_DNG);

	nprocs = get_processor();
	if (nprocs < 2) nprocs = 2;

	pthread_t tid[nprocs];

	thd_arg = (struct thread_arg) {
		.serv_adr = (struct sockaddr_in) {
			.sin_family = AF_INET,
			.sin_addr.s_addr = inet_addr(argv[2]),
			.sin_port = htons(atoi(argv[3]))
		},	//Do not use serv_adr to socket function directly,
			//remained member(or byte) isn't filled with zero
		.q = &header_queue,
		.epfd = epfd
	};

	int check;
	printf("number of processors: %ld \n", nprocs);
	for (long i = 0; i < nprocs; i++) {
		if ((check = pthread_create(
				&tid[i], NULL, worker_thread, &thd_arg
												)) != 0 ) {
			err_msg("pthread_create() error: %d", check, ERR_NRM);
		}
	}

	for (long i = 0; i < nprocs; i++) {
		if ((check = pthread_join(tid[i], &ret)) != 0) {
			err_msg("pthread_join() error: %d", check, ERR_NRM);
		}
	}

	queue_release(&header_queue);

	close(proxy_sock);

	return 0;
}

void *worker_thread(void *ptr)
{
	struct thread_arg thd_arg = *(struct thread_arg *)ptr;

	int proxy_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	struct queue *q;
	int flag;

	struct epoll_event *ep_events;
	struct epoll_event event;
	int epfd, event_cnt;

	int str_len;

	char buf[HEADER_SIZE];

	proxy_sock = thd_arg.proxy_sock;

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr = thd_arg.serv_adr;

	q = thd_arg.q;
	epfd = thd_arg.epfd;

	printf("[worker: %lu] serv_adr: %s:%hd \n", 
			pthread_self() % 100, 
			inet_ntop(	
				AF_INET, &serv_adr.sin_addr, 
				(char [INET_ADDRSTRLEN]) { /* empty */ }, 
				INET_ADDRSTRLEN
			), ntohs(serv_adr.sin_port)
	);

	while (true)
	{
		event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
		if (event_cnt == -1) {
			err_msg("epoll_wait() error", ERR_CHK);

			return (void *)EXIT_FAILURE;
		}

		printf("[worker %lu] ", pthread_self() % 100);

		for (int i = 0; i < event_cnt; i++)
		{
			if (ep_events[i].data.fd == proxy_sock) {
				clnt_sock = accept(
						proxy_sock, (struct sockaddr*)&clnt_adr, (int[1]) { sizeof(clnt_adr) }
				);

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
				
				epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
				
				printf("connected client: %d \n", clnt_sock);
			} else {
				str_len = read(ep_events[i].data.fd, buf, HEADER_SIZE);
				if (str_len == 0) {
					epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
					close(ep_events[i].data.fd);
					printf("closed client: %d \n",ep_events[i].data.fd);
					break;
				} else if (str_len < 0) {
					if (errno == EAGAIN)
						break;
				} else {
					write(ep_events[i].data.fd, buf, str_len);
				}
			}

			continue;
		}
	}

	return (void *)EXIT_SUCCESS;
}

long get_processor(void)
{
	long nprocs;

#ifdef _SC_NPROCESSORS_ONLN
	nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs < 0) {
		if (errno == EINVAL) {
			err_msg("sysconf: _SC_NPROCESSORS_ONLN not supported", ERR_NRM);
		} else {
			err_msg("sysconf error", ERR_CTC);
		}
	}
#else
	err_msg("_SC_NPROCESSORS_ONLN doesn't exist", ERR_NRM);
	nprocs = 2;
#endif

	return nprocs;
}
