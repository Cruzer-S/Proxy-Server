#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "handler/error.h"
#include "handler/socket.h"

#define BUF_SIZE	(4096)

int main(int argc, char *argv[])
{
	int sock;
	char buf[BUF_SIZE];
	int wr_len, rd_len, count;

	if (argc != 3)
		err_msg("usage: %s <ip address> <port>", ERR_DNG, argv[0]);

	sock = connect_socket(argv[1], atoi(argv[2]));
	if (sock < 0)
		err_msg("connect_socket() error: %d", ERR_CTC, sock);

	while (true)
	{
		fputs("Input: ", stdout);
		fgets(buf, BUF_SIZE, stdin);

		if (strlen(buf) == 1)
			break;
		
		wr_len = write(sock, buf, strlen(buf));

		rd_len = 0;
		while (rd_len < wr_len) {
			count = read(sock, &buf[rd_len], BUF_SIZE - 1);
			if (count == -1)
				err_msg("read() error", ERR_CTC);

			rd_len += count;
		}

		buf[rd_len] = 0;
		printf("Message: %s", buf);
	}

	close(sock);

	return 0;
}
