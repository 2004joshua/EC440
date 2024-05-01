#include <stdio.h>
#include <pthread.h> 
#include <assert.h> 
#include <stdlib.h>
#include <setjmp.h>
#include "ec440threads.h" 

#define NUM_THREADS 10 

pthread_barrier_t barrier; 
pthread_mutex_t mutex; 

// shared value
int value = 0; 

void* thread_function(void* arg) {
  pthread_mutex_lock(&mutex);

  value++; 

  pthread_mutex_unlock(&mutex); 

  pthread_barrier_wait(&barrier); 

  pthread_mutex_lock(&mutex) ;
  
  printf("Shared data after barrier: %d\n", value);

  pthread_mutex_unlock(&mutex); 

  return NULL; 
}


int main() {

  pthread_t threads[NUM_THREADS]; 
  pthread_barrier_init(&barrier, NULL, NUM_THREADS); 
  pthread_mutex_init(&mutex, NULL);

  for(int i = 0; i < NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, thread_function, NULL); 
  } 

  for(int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL); 
  }

  pthread_barrier_destroy(&barrier); 
  pthread_mutex_destroy(&mutex); 

  return 0; 
}
