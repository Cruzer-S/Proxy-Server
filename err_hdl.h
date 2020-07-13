#ifndef __ERR_HDL_H__
#define __ERR_HDL_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>			//exit
#include <errno.h>			//errno
#include <stdarg.h>			//VLA

#define MAXLINE	(1024)

#define ECODE_EXIT 		(0b0001)
#define ECODE_ERRNO		(0b0010)

#define ERR_NRM	0							//normal error
#define ERR_DNG (ECODE_EXIT)				//danger error
#define ERR_CTC (ECODE_EXIT | ECODE_ERRNO)	//critical error

void err_msg(const char *, int, ...);

#endif
