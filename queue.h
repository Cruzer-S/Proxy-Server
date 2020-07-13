#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdlib.h>		//malloc
#include <stdbool.h>	//bool

#include <string.h>		//memcpy

struct queue {
	void **data;

	size_t first;
	size_t last;

	size_t size;
	size_t length;

	bool reverse;
};

int queue_create(struct queue* , size_t , size_t );

bool queue_push(struct queue* , void* );

bool queue_pop(struct queue* , void* );

size_t queue_size(struct queue* );

bool queue_empty(struct queue* );

void queue_release(struct queue* );

#endif
