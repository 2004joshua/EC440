#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include "ec440threads.h"



pthread_mutex_t mutex;
int shared_data = 0;

void* thread_func(void* arg) {
  for (int i = 0; i < 5; i++) {
    pthread_mutex_lock(&mutex);
    int local = shared_data;
    printf("Thread %ld accessing shared data: %d\n", (long)arg, local);
    local++;
    usleep(100000); // Simulate data processing.
    shared_data = local;
    assert(shared_data == local); // Confirm no other thread has modified shared_data.
    pthread_mutex_unlock(&mutex);
  }
  return NULL;
}

int main() {
  pthread_t threads[3];

  pthread_mutex_init(&mutex, NULL);

  for (long i = 0; i < 3; i++) {
    pthread_create(&threads[i], NULL, thread_func, (void*)i);
  }

  for (int i = 0; i < 3; i++) {
    pthread_join(threads[i], NULL);
  }

  pthread_mutex_destroy(&mutex);

  assert(shared_data == 15); // Confirm final value of shared_data.
  printf("Final shared data value: %d\n", shared_data);
  return 0;
}
