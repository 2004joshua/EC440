#include <stdio.h> 
#include "ec440threads.h"
#include <assert.h> 
#include <pthread.h> 
#include <setjmp.h>
#include <time.h> 
#include <stdlib.h>
#include <errno.h> 

void* start_routine(void* args){
	
	printf("Hello World! from %d\n",(int)pthread_self()); 
	pthread_exit(NULL); 
}
int main(){


	pthread_t t1; 
	pthread_create(&t1, NULL, start_routine, NULL);
	return 0; 
}
