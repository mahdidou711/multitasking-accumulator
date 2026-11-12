#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>

#define ERROR_INIT 1
#define ERROR_SUCCESS 0
#define SEMAPHORE_INITIAL_VALUE 0

pthread_t thread_1;
sem_t semaphore;
int thread_error;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *produce(void *params)
{
    int rc;
    (void)params;

    printf("Trying to own the mutex\n");
    rc = pthread_mutex_lock(&mutex);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(rc));
        thread_error = 1;
        return NULL;
    }
    printf("Owns the mutex\n");
    rc = pthread_mutex_unlock(&mutex);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(rc));
        thread_error = 1;
        return NULL;
    }
    printf("Mutex released\n");
    if (sem_post(&semaphore) == -1)
    {
        perror("sem_post");
        thread_error = 1;
        return NULL;
    }
    printf("Semaphore posted\n");
    printf("Trying to gets the semaphore\n");
    while (sem_wait(&semaphore) == -1)
    {
        if (errno != EINTR)
        {
            perror("sem_wait");
            thread_error = 1;
            return NULL;
        }
    }
    printf("Semaphore taken\n");
    printf("Thread ended\n");
    return NULL;
}

int main(void)
{
    int rc;
    int status = ERROR_SUCCESS;

    if (sem_init(&semaphore, 0, SEMAPHORE_INITIAL_VALUE) == -1)
    {
        perror("sem_init");
        return ERROR_INIT;
    }

    printf("Creating the thread\n");
    rc = pthread_create(&thread_1, NULL, produce, NULL);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        status = ERROR_INIT;
    }
    else
    {
        printf("Waiting the thread end\n");
        rc = pthread_join(thread_1, NULL);
        if (rc != 0)
        {
            fprintf(stderr, "pthread_join: %s\n", strerror(rc));
            /* The thread may still be using the semaphore; do not destroy it. */
            return ERROR_INIT;
        }
        if (thread_error)
            status = ERROR_INIT;
    }

    printf("Deleting the semaphore\n");
    if (sem_destroy(&semaphore) == -1)
    {
        perror("sem_destroy");
        status = ERROR_INIT;
    }
    printf("Process ended\n");
    return status;
}
