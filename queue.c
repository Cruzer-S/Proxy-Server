#include "queue.h"

int queue_create(struct queue* q, size_t size, size_t length)
{
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
	size_t first = q->first, last = q->last;

	if (q->reverse)
		if (first == last)
			return false;

	memcpy(q->data[last], data, q->size);

	if ((last + 1) >= q->length) {
		q->last = 0U;
		q->reverse = true;
	}

	return true;
}

bool queue_pop(struct queue* q, void* data)
{
	size_t first = q->first, last = q->last;

	if (!q->reverse)
		if (first == last)
			return false;

	memcpy(data, q->data[first], q->size);

	if ((first + 1) >= q->length) {
		q->first = 0U;
		q->reverse = false;
	}

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
}
