#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <signal.h>

typedef void Sigfunc(int);

Sigfunc *sigset(int , Sigfunc *);

#endif