#include <stdio.h>
#include <pthread.h>

pthread_mutex_t mutex;
int critical_data = 0;

void *modify_data(void *arg) {
  pthread_mutex_lock(&mutex);
  critical_data++;

  printf("Critical data modified to %d by thread\n", critical_data);
  pthread_mutex_unlock(&mutex);
  return NULL;
}

int main() {

  pthread_t thread1, thread2;
  pthread_mutex_init(&mutex, NULL);

  pthread_create(&thread1, NULL, modify_data, NULL);
  pthread_create(&thread2, NULL, modify_data, NULL);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  pthread_mutex_destroy(&mutex); 
  return 0;
}

