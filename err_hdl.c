#include "err_hdl.h"

void err_msg(const char *fmt_msg, int type, ...)
{
	char buf[MAXLINE];

	va_list ap;

	va_start(ap, type);

	vsnprintf(buf, MAXLINE - 1, fmt_msg, ap);

	fflush(stdout);
	
	fputs(buf, stderr);

	fflush(NULL);

	va_end(ap);

	switch (type)
	{
	case ERR_NRM:
		break;

	case ERR_DNG:
		exit(1);

	case ERR_CTC:
		exit(1);
	}
}
