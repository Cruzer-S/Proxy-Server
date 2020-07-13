#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "err_hdl.h"

#define BUF_SIZE	(4096)

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_adr;
	char buf[BUF_SIZE];
	int wr_len, rd_len;

	if (argc != 3)
		err_msg("usage: %s <ip address> <port>", ERR_DNG, argv[0]);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		err_msg("socket(2) error", ERR_CTC);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));

	if (connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		err_msg("connect(2) error", ERR_CTC);

	while (true)
	{
		fputs("Input: ", stdout);
		fgets(buf, BUF_SIZE, stdin);

		if (strlen(buf) == 1)
			break;
		
		wr_len = write(sock, buf, strlen(buf));
	}

	close(sock);

	return 0;
}
