#include "tls.h"
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h> 
#include <signal.h>
#include <sys/mman.h>
#include <string.h> 
#include <stdint.h>

//list of thread ids setup:

struct pages{
  void* address;
  unsigned int overlap; 
};

struct tls {
    pthread_t tid;                 		// thread id
    unsigned int size;             		// specified size
    unsigned int page_count;       		// how many pages this tls can have based off size
    struct tls *next; 
    struct pages p_arr[];          		// flexible array member
};
 
//global variables:

struct tls *list; 						//global linked list
unsigned int page_size; 				//getpagesize()
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;		//locking and unlocking for each function to prevent race conditions

//helper functions:

void sig_handler_fault(int sig, siginfo_t *si, void *unused) {
	char* fault = (char*)si->si_addr;

	struct tls *tls_fault = list; 
	while(tls_fault != NULL){
		for(int i = 0; i < tls_fault->page_count; i++){
			char* start = (char*)tls_fault->p_arr[i].address; 
			char* end = start + page_size; 
			if(fault >= start && fault < end){
				fprintf(stderr,"ERROR: thread %lu caused this error!\n", (unsigned long)tls_fault->tid);
				pthread_exit(NULL); 
			}
		}

		tls_fault = tls_fault->next; 
	}

	signal(SIGSEGV, SIG_DFL); 
	signal(SIGBUS, SIG_DFL); 
	raise(sig); 
}

void initialize(){
	list = NULL; 
	page_size = getpagesize(); 

	struct sigaction s; 
	memset(&s, 0, sizeof(s)); 
	s.sa_flags = SA_SIGINFO; 
	s.sa_sigaction = sig_handler_fault; 
	sigaction(SIGBUS, &s, NULL);
	sigaction(SIGSEGV, &s, NULL);
}

void protect(struct pages* p){
	if(mprotect((void*)p->address, page_size, PROT_NONE)){
		fprintf(stderr, "ERROR: protect failed!\n");
		exit(1); 
	}
}

void unprotect(struct pages* p){
	if(mprotect((void*)p->address, page_size, PROT_READ | PROT_WRITE)){
		fprintf(stderr, "ERROR: unprotect failed\n"); 
	}
}

//tls functions: 

int tls_create(unsigned int size){

	if(list == NULL){
		initialize(); 
	}

	pthread_mutex_lock(&list_mutex); 
	struct tls *current = list; 
	while(current != NULL){
		if(current->tid == pthread_self()){
			fprintf(stderr,"ERROR: Current thead has a TLS!\n");
			pthread_mutex_unlock(&list_mutex); 
			return -1; 
		}
		current = current->next; 
	}

	unsigned int count = (size + page_size - 1) / page_size;
	struct tls *new_tls = (struct tls*)malloc(sizeof(struct tls) + count * sizeof(struct pages)); 
	
	if(new_tls == NULL){
		fprintf(stderr, "ERROR: Failure to allocate new tls!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	new_tls->tid = pthread_self(); 
	new_tls->size = size; 
	new_tls->page_count = count; 

	for(int i = 0; i < new_tls->page_count; i++){
		new_tls->p_arr[i].address = mmap(NULL, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE,  0, 0); 
		if(new_tls->p_arr[i].address == MAP_FAILED){
			
			fprintf(stderr, "ERROR: Failure to map page address!\n"); 
			
			for(int j = 0; j <= i; j++){
				munmap(new_tls->p_arr[j].address, page_size);
			}

			free(new_tls); 
			pthread_mutex_unlock(&list_mutex); 
			return -1;
		}

		new_tls->p_arr[i].overlap = 1; 
	}

	new_tls->next = list; 
	list = new_tls; 

	pthread_mutex_unlock(&list_mutex); 
	return 0; 
}

int tls_destroy(){

	pthread_mutex_lock(&list_mutex); 
	pthread_t cthread = pthread_self(); 

	struct tls *current = list; 
	struct tls *prev = NULL; 

	while(current != NULL){
		if(current->tid == cthread){
			break; 
		}
		prev = current; 
		current = current->next; 
	}

	if(current == NULL){
		fprintf(stderr, "ERROR: Failure to locate tls with thread!\n");
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	for(int i = 0; i < current->page_count; i++){
		if(current->p_arr[i].overlap == 1){
			if(munmap(current->p_arr[i].address, page_size) != 0){
				fprintf(stderr, "ERROR: Failure to use munmap on page %d!\n", i); 
				pthread_mutex_unlock(&list_mutex);
				return -1; 
			}
			else{
				__atomic_fetch_sub(&current->p_arr[i].overlap, 1, __ATOMIC_SEQ_CST); 
			}
		}
	}

	if(prev == NULL){
		list = current->next; 
	}
	else{
		prev->next = current->next; 
	}

	free(current); 
	pthread_mutex_unlock(&list_mutex); 
	return 0; 
}

int tls_read(unsigned int offset, unsigned int length, char *buffer){

	pthread_mutex_lock(&list_mutex); 
	pthread_t ctid = pthread_self(); 
	struct tls *ctls = list; 

	while(ctls != NULL){
		if(ctid == ctls->tid){
			break; 
		}
		ctls = ctls->next; 
	}

	if(ctls == NULL){
		fprintf(stderr, "ERROR: Failure to locate tls you want to read!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	if((ctls->page_count * page_size) < (offset + length)){
		fprintf(stderr, "ERROR: Out of bounds!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	for(int i = 0; i < ctls->page_count; i++){
		unprotect(&ctls->p_arr[i]); 
	}

	unsigned int cnt, idx; 
	for(cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx){
		unsigned int pn = idx / page_size;
		unsigned int poff = idx % page_size; 
		char *src = ((char*)ctls->p_arr[pn].address) + poff;
		buffer[cnt] = *src; 
	}

	for(int i = 0; i < ctls->page_count; i++){
		protect(&ctls->p_arr[i]); 
	}

	pthread_mutex_unlock(&list_mutex); 
	return 0; 
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer){
	
	pthread_mutex_lock(&list_mutex); 
	pthread_t ctid = pthread_self(); 
	struct tls *ctls = list; 

	while(ctls != NULL){
		if(ctid == ctls->tid){
			break; 
		}
		ctls = ctls->next; 
	}

	if(ctls == NULL){
		fprintf(stderr, "ERROR: Failure to locate tls you want to read!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	if((ctls->page_count * page_size) < (offset + length)){
		fprintf(stderr, "ERROR: Out of bounds!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	for(int i = 0; i < ctls->page_count; i++){
		unprotect(&ctls->p_arr[i]); 
	}

	unsigned int cnt, idx; 
	for(cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx){
		unsigned int index = idx / page_size;
		unsigned int page_off = idx % page_size;

		if(ctls->p_arr[index].overlap > 1){
			__atomic_fetch_sub(&ctls->p_arr[index].overlap,1,__ATOMIC_SEQ_CST); 

			void* new_addr = mmap(NULL, page_size, PROT_WRITE | PROT_READ, MAP_ANON | MAP_PRIVATE, 0, 0); 
			memcpy(new_addr, ctls->p_arr[index].address, page_size); 

			ctls->p_arr[index].address = new_addr; 
			ctls->p_arr[index].overlap = 1; 
		} 
		char *src = ((char*)ctls->p_arr[index].address) + page_off;
		*src = buffer[cnt]; 
	}
	
	for(int i = 0; i < ctls->page_count; i++){
		protect(&ctls->p_arr[i]); 
	}

	pthread_mutex_unlock(&list_mutex); 
	return 0; 
}

int tls_clone(pthread_t tid) {
	
	pthread_mutex_lock(&list_mutex); 
	
	if(pthread_self() == tid){
		fprintf(stderr, "ERROR: Cannot clone tls with this thread id!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	struct tls *parent = list; 
	while(parent != NULL){
		if(parent->tid == pthread_self()){
			fprintf(stderr, "ERROR: Current thread has a tls!\n");
			pthread_mutex_unlock(&list_mutex); 
			return -1;
		}
		parent = parent->next; 
	}
	
	parent = list; 
	while(parent != NULL){
		if(parent->tid == tid){
			break; 
		}
		parent = parent->next; 
	}

	if(parent == NULL){
		fprintf(stderr, "ERROR: Failure to find tls to copy!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	struct tls *clone_tls = malloc(sizeof(struct tls) + parent->page_count * sizeof(struct pages)); 
	
	if(clone_tls == NULL){
		fprintf(stderr, "ERROR: Failure to allocate clone tls!\n"); 
		pthread_mutex_unlock(&list_mutex); 
		return -1; 
	}

	clone_tls->tid = pthread_self(); 
	clone_tls->page_count = parent->page_count; 
	clone_tls->size = parent->size; 
	clone_tls->next = NULL; 

	for(int i = 0; i < clone_tls->page_count; i++){
		//incrementing overlap value and setting up address for clone
		__atomic_fetch_add(&parent->p_arr[i].overlap,1,__ATOMIC_SEQ_CST); 
		clone_tls->p_arr[i].address = parent->p_arr[i].address; 
		clone_tls->p_arr[i].overlap = 1; 
	}

	clone_tls->next = list; 
	list = clone_tls; 

	pthread_mutex_unlock(&list_mutex); 
    return 0;
}





