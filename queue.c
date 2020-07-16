$)C#include "queue.h"

int queue_create(struct queue* q, size_t size, size_t length, bool thread_safety)
{
	if (thread_safety)
		if (pthread_mutex_init(&q->mtx, NULL) != 0))
			return -1;
	
	q->data = malloc(sizeof(void*) * length);
	if (q == NULL)
		return -1;

	for (size_t i = 0; i < length; i++) {
		q->data[i] = malloc(size);

		if (q->data[i] == NULL)
			return -1;
	}

	q->first = q->last = 0U;
	q->size = size;
	q->length = length;
	q->reverse = false;

	return 0;
}

bool queue_push(struct queue* q, void* data)
{
	pthread_mutex_lock(&q->mtx);
	
	size_t first = q->first, last = q->last;

	if (q->reverse) {
		if (first == last) {
			pthread_mutex_unlock(&q->mtx);	
			return false;
		}
	}

	memcpy(q->data[last], data, q->size);

	if ((last + 1) >= q->length) {
		q->last = 0U;
		q->reverse = true;
	}
	
	pthread_mutex_unlock(&q->mtx);

	return true;
}

bool queue_pop(struct queue* q, void* data)
{
	pthread_mutex_lock(&q->mtx);
	
	size_t first = q->first, last = q->last;

	if (!q->reverse) {
		if (first == last) {
			pthread_mutex_unlock(&q->mtx);
			return false;
		}
	}

	memcpy(data, q->data[first], q->size);

	if ((first + 1) >= q->length) {
		q->first = 0U;
		q->reverse = false;
	}
	
	pthread_mutex_lock(&q->mtx);
	return true;
}

size_t queue_size(struct queue* q)
{
	if (q->reverse)
		return (5 - q->last) - q->first;
	else
		return q->last - q->first;
}

bool queue_empty(struct queue* q)
{
	return (q->first == q->last) ? (true) : (false);
}

void queue_release(struct queue* q)
{
	for (size_t i = 0; i < q->length; i++)
		free(q->data[i]);

	free(q->data);

	q->size = q->length = 0U;
	q->first = q->last = 0U;
	q->reverse = false;
	
	pthread_mutex_destroy(&q->mtx);
}
