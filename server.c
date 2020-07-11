#include <stdio.h>
#include "err_hdl.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
		err_msg("usage: %s <ip address> \n", ERR_DNG, argv[0]);

	printf("server address: %s \n", argv[1]);

	return 0;
}
