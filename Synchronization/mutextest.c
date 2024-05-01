#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include<assert.h>
#include <setjmp.h>
#include "ec440threads.h"




pthread_mutex_t mutex;

void *thread_func(void *arg) {
    int thread_id = *((int *)arg);
    if (thread_id != 2) {
        pthread_mutex_lock(&mutex);
        printf("Thread %d acquired the mutex.\n", thread_id);
        // Simulate some work
        usleep(1000000);
        printf("Thread %d released the mutex.\n", thread_id);
        pthread_mutex_unlock(&mutex);
    } else {
	pthread_mutex_lock(&mutex);
        printf("Thread %d acquired the mutex.\n", thread_id);
        // Hold the mutex for a longer time to let other threads contend for it
        usleep(2000000);
        printf("Thread %d released the mutex.\n", thread_id);
	pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main() {
    pthread_t threads[5];
    int thread_ids[5];

    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < 5; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}

