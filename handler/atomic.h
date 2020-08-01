#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include <pthread.h>    // mutex
#include <stdio.h>      // vsnprintf
#include <stdlib.h>     // malloc, free
#include <stdarg.h>     // Variable Argument

#define MAX_BUF (1024)

int init_atomic_alloc(void);

void *atomic_alloc(size_t );

void atomic_free(vodi *);

void atomic_print(const char *, ...);

void release_atomic_alloc(void);

#endif