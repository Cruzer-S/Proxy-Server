#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

struct thead_arg {
	struct queue* q;
};

void *worker_thread(void* );

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
	err_msg("_SC_NPROCESSORS_ONLN not exist", ERR_NRM);
	nprocs = 4;
#endif

	return nprocs;
}

int main(int argc, char *argv[])
{
	int proxy_sock;
	struct sockaddr_in proxy_adr;
	struct queue header_queue;

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

	pthread_t tid;
	long nprocs = get_processor();
	if (nprocs < 2) nprocs = 2;

	printf("number of processors: %ld \n", nprocs);
	for (long i = 0; i < nprocs; i++) {
		if (pthread_create(
				&tid, NULL, worker_thread, 
				(void*)&(struct sockaddr_in) {
					.sin_family = AF_INET,
					.sin_addr.s_addr = inet_addr(argv[2]),
					.sin_port = htons(atoi(argv[3]))
					}) != 0) {
			err_msg("pthread_create() error", ERR_NRM);
		}

	}

	queue_release(&header_queue);

	close(proxy_sock);

	return 0;
}

void *worker_thread(void* ptr)
{
	struct sockaddr_in serv_adr = *(struct sockaddr_in*)ptr;

	printf("[%lu] serv_adr: %s:%hd \n", 
			pthread_self() % 100, 
			inet_ntop(AF_INET, &serv_adr.sin_addr, 
				(char [INET_ADDRSTRLEN]){[0] = '\0'}, 
				INET_ADDRSTRLEN
			), ntohs(serv_adr.sin_port)
	);
}
