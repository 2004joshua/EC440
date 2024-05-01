#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <setjmp.h>
#include "ec440threads.h"  // Include your barrier implementation

#define NUM_THREADS 5
#define NUM_CYCLES 3

pthread_barrier_t barrier;

void* thread_func(void* arg) {
  int cycle = *((int*)arg);
  printf("Thread %ld entering barrier at cycle %d\n", pthread_self(), cycle);
  int rc = pthread_barrier_wait(&barrier);
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
    printf("Barrier wait failed: %d\n", rc);
    pthread_exit(NULL);
  }
  printf("Thread %ld exiting barrier at cycle %d\n", pthread_self(), cycle);
  pthread_exit(NULL);
}

int main() {
  pthread_t threads[NUM_THREADS];
  int cycle, i, rc;

  for (cycle = 0; cycle < NUM_CYCLES; cycle++) {
    printf("Initializing barrier, cycle %d\n", cycle);
    rc = pthread_barrier_init(&barrier, NULL, NUM_THREADS);
    assert(rc == 0); // Assert initialization success

    for (i = 0; i < NUM_THREADS; i++) {
      rc = pthread_create(&threads[i], NULL, thread_func, &cycle);
      assert(rc == 0); // Assert thread creation success
    }

    for (i = 0; i < NUM_THREADS; i++) {
      pthread_join(threads[i], NULL);
    }

    printf("Destroying barrier, cycle %d\n", cycle);
    rc = pthread_barrier_destroy(&barrier);
    assert(rc == 0); // Assert destruction success
  }

  printf("Test completed successfully.\n");
  return 0;
}
