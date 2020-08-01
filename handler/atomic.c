#define "atomic.h"

static pthread_mutex_t mutex_alloc, mutex_print;

int init_atomic_alloc(void)
{
    if (pthread_mutex_init(&mutex_alloc, NULL) != 0)
        return -1;

    if (pthread_mutex_init(&mutex_print, NULL) != 0)
        return -2;
}

void *atomic_alloc(size_t size)
{
    void *ptr;

    pthread_mutex_lock(&mutex_alloc);
    ptr = malloc(size);
    pthread_mutex_unlock(&mutex_alloc);

    return ptr;
}

void atomic_print(const char *str, ...)
{
    va_list ap;

    va_start(ap, str);

    pthread_mutex_lock(&mutex_print);
    printf(str, ap);
    pthread_mutex_unlock(&mutex_print);

    va_end(ap);
}

void atomic_free(void *ptr)
{
    pthread_mutex_lock(&mutex_alloc);
    free(ptr);
    pthread_mutex_unlock(&mutex_alloc);
}

void release_atomic_alloc(void)
{
    pthread_mutex_destroy(&mutex_alloc);
    pthread_mutex_destroy(&mutex_print);
}