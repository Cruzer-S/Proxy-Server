#include "err_hdl.h"

void err_msg(const char *fmt_msg, int type, ...)
{
	char buf[MAXLINE];

	va_list ap;

	va_start(ap, type);

	vsnprintf(buf, MAXLINE - 1, fmt_msg, ap);

	va_end(ap);

	if (type & ECODE_ERRNO)
		snprintf(buf + strlen(buf), MAXLINE - strlen(buf) - 1, 
				": %s", strerror(errno));

	strcat(buf, "\n");

	fflush(stdout);
	fputs(buf, stderr);
	fflush(NULL);

	if (type & ECODE_EXIT)
		exit(1);
}
