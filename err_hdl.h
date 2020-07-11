#ifndef __ERR_HDL_H__
#define __ERR_HDL_H__

#include <stdio.h>
#include <stdlib.h>			//exit
#include <errno.h>			//errno
#include <stdarg.h>			//VLA

#define MAXLINE	(1024)

#define ERR_NRM	(1)		//normal error
#define ERR_DNG (2)		//danger error
#define ERR_CTC (3)		//critical error

void err_msg(const char *, int, ...);

#endif
