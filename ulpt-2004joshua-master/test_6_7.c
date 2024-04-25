#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include "ec440threads.h"



pthread_mutex_t mutex;
int critical_data = 0;

void* thread_func(void* arg) {
  pthread_mutex_lock(&mutex);
  printf("Thread %ld entering critical section.\n", (long)arg);
  int local = critical_data;
  local++;
  sleep(1); // Simulate work in critical section.
  critical_data = local;
  assert(critical_data == local); // Confirm no other thread has modified critical_data.
  printf("Thread %ld leaving critical section.\n", (long)arg);
  pthread_mutex_unlock(&mutex);
  return NULL;
}

int main() {
  pthread_t thread1, thread2;

  pthread_mutex_init(&mutex, NULL);

  pthread_create(&thread1, NULL, thread_func, (void*)1);
  pthread_create(&thread2, NULL, thread_func, (void*)2);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  pthread_mutex_destroy(&mutex);

  assert(critical_data == 2); // Ensure final value is correct.
  printf("Critical data: %d\n", critical_data);
  return 0;
}
