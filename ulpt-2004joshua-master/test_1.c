#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include "ec440threads.h"

pthread_mutex_t mutex;
volatile int in_critical_section = 0;

void* thread_func(void* arg) {
    int result;

    printf("Thread %ld attempting to acquire the mutex.\n", (long)arg);
    result = pthread_mutex_lock(&mutex);
    if (result != 0) {
        fprintf(stderr, "Thread %ld failed to lock the mutex with error: %d\n", (long)arg, result);
        return NULL;
    }

    in_critical_section++;
    assert(in_critical_section == 1); // Ensure no other thread is in the critical section.
    printf("Thread %ld has acquired the mutex.\n", (long)arg);

    sleep(2); // Hold the mutex for 2 seconds to simulate work.

    in_critical_section--;
    assert(in_critical_section == 0); // Ensure the critical section is exited correctly.

    printf("Thread %ld releasing the mutex.\n", (long)arg);
    result = pthread_mutex_unlock(&mutex);
    if (result != 0) {
        fprintf(stderr, "Thread %ld failed to unlock the mutex with error: %d\n", (long)arg, result);
    }

    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    
    int init_result = pthread_mutex_init(&mutex, NULL);
    if (init_result != 0) {
      fprintf(stderr, "Failed to initialize mutex with error: %d\n", init_result);
      return 1;
      } else {
        printf("Mutex successfully initialized.\n");
      }


    pthread_create(&thread1, NULL, thread_func, (void*)1);
    sleep(1); // Ensure thread1 grabs the mutex first.
    pthread_create(&thread2, NULL, thread_func, (void*)2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&mutex);

    return 0;
}
