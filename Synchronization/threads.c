#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h> 
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include "ec440threads.h"

/* Givens */

// This library will support a maximum of 128 threads
#define MAX_THREADS 128 
// Each thread will have a size of 32768 bytes
#define THREAD_STACK_SIZE (1<<15)
// time slice allotted for the thread in usec
#define quantum (50 * 1000)

enum state { 
  TS_EXITED,
  TS_RUNNING,
  TS_READY,
  TS_UNINITIALIZED,
  TS_BLOCKED // Assignment 5
};

/*
Mutex and Barrier data structures for Assignment 5
*/

typedef struct {
  int _count; 
  int _lock; 
  int _init; 
  int _owner; 
  int thread_queue[MAX_THREADS];
} my_mutex;

typedef union {
  pthread_mutex_t mutex; 
  my_mutex vars; 

} mutex_t;

struct node_t{
  pthread_t id; 
  struct node_t *next; 
};

typedef struct {
	uint8_t count; 
	uint8_t limit; 
  uint8_t init; 	
  struct node_t* head; 
}my_barrier; 

typedef union {
  pthread_barrier_t barrier;
  my_barrier vars;
} barrier_t;

/*
The thread control block (TCB) holds the necessary info about the thread
and how to manage it 
*/

struct tcb {
  // thread id
  pthread_t id; 
  // thread stack pointer
  uint8_t *stack; 
  // current status of this thread
  enum state status; 
  // stores the thread's context like the reg set, pc and others
  jmp_buf context; 
  // return the value when exited
  void* retval;
};

// global variables and data structures
struct tcb threads[MAX_THREADS]; 
int CurrentThread = 0; 
int ActiveThreads = 0; 

/* This scheduler is an implementation of the round robin scheduler */
static void schedule(int signal) { 
  int PreviousThread = CurrentThread; 
  int found = 0; 

  // find a thread thats ready to operate
  for(int i = 0; i < MAX_THREADS; i++) { 
    int next = (CurrentThread + i) % MAX_THREADS; 

    if(threads[next].status == TS_READY) {
      CurrentThread = next; 
      found = 1; 
      break; 
    }
  }

  // no thread was ready to run 
  if(found == 0) {
    return; 
  }

  struct tcb* prev = &threads[PreviousThread];
  struct tcb* current = &threads[CurrentThread];

  // case 1: previous wasn't exited
  if(prev->status != TS_EXITED && prev->status != TS_BLOCKED) {
    if(setjmp(prev->context) == 0) {
      prev->status = TS_READY;
      current->status = TS_RUNNING;
      longjmp(current->context,1);
    }
  }
  // case 2: previous was blocked
  else if(prev->status == TS_BLOCKED){
    if(setjmp(prev->context) == 0) {
      longjmp(current->context, 1);
    }
    else {
      return;
    }
  }
  // case 3: previous was exited
  else{
    current->status = TS_RUNNING;
    longjmp(current->context,1);

     // free the previous thread's stack and decrement the active threads
    if(threads[PreviousThread].stack != NULL) {
      free(threads[PreviousThread].stack);
      threads[PreviousThread].stack = NULL;
    }

    ActiveThreads--;
  }
}

static void schedule_wrap(int signal) {
  schedule(signal); 
}

static void scheduler_init() {
  CurrentThread = 0; 

  // setup the global array containing the thread control blocks
  for(int i = 1; i < 128; i++) {
    threads[i].status = TS_UNINITIALIZED;
  }

  threads[0].status = TS_RUNNING;
  threads[0].stack = NULL; 
  threads[0].id = (pthread_t)0; 

  ActiveThreads++; 

  struct sigaction sa; 
  memset(&sa, 0, sizeof(sa)); 
  sa.sa_handler = schedule_wrap; 
  sigaction(SIGALRM, &sa, NULL); 

  struct itimerval timer;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = quantum;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = quantum;
        
  setitimer(ITIMER_REAL, &timer, NULL);	
}

static void schedule(int signal) __attribute__((unused)); 

// Pthread Functions 

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {

  /*
  The pthread_create() function creates a new thread within a process. Upon successful 
  completion, pthread_create() stores the ID of the created thread in the location referenced by
  thread. In our implementation, the second argument (attr) shall always be NULL. The thread 
  is created and executes start_routine with arg as its sole argument. If the start_routine 
  returns, the effect shall be as if there was an implicit call to pthread_exit(). Note that the 
  thread in which main() was originally invoked differs from this. When it returns from main(), the
  effect shall be as if there was an implicit call to exit() using the return value of main() as the 
  exit status.
  */
  
  static int first_call = 1; 
  if(first_call == 1) {
    first_call = 0;
    scheduler_init();
  }

  if(ActiveThreads > MAX_THREADS) {
    fprintf(stderr, "ERROR: Too many active threads!\n"); 
    return -1; 
  }

  // find an uninitialized index within the array
  int idx = 0;
  for(int i = 1; i < MAX_THREADS; i++) {
    if(threads[i].status == TS_UNINITIALIZED) {
      idx = i; 
      break; 
    }
  }

  if(idx == 0) {
    fprintf(stderr, "ERROR: There is currently no open index to create the thread!\n"); 
    return -1; 
  }

  struct tcb* new_thread = &threads[idx]; 

  // setting up the thread control block parameters
  new_thread->status = TS_READY;

  new_thread->stack = (uint8_t *)malloc(THREAD_STACK_SIZE); 
  if(!new_thread->stack) {
    fprintf(stderr, "ERROR: Failure to malloc the thread stack for thread %d!\n", idx) ; 
    return -1; 
  }

  new_thread->id = (pthread_t)idx; 
  *thread = new_thread->id; 

  new_thread->retval = NULL; 

  void* top_of_stack = new_thread->stack + THREAD_STACK_SIZE - 8; 
  *(uintptr_t*)top_of_stack = (uintptr_t)pthread_exit; 

  /* 
    Setting up the context: 
      * registers for the top
      * program counter
      * start routine
      * start routine arguments
  */

  set_reg(&threads[idx].context, JBL_RSP, (unsigned long)top_of_stack);
  set_reg(&threads[idx].context, JBL_PC, (unsigned long)start_thunk);
  set_reg(&threads[idx].context, JBL_R12, (unsigned long)start_routine);
  set_reg(&threads[idx].context, JBL_R13, (unsigned long)arg);

  ActiveThreads++; 
  return 0;   
}

void pthread_exit(void *retval) {

  /*
  The pthread_exit() function terminates the calling thread, and returns a value via retval that 
  is available to another thread in the same process that calls pthread_join().  Note, the value 
  pointed to by retval should not be located on the thread’s stack, since the contents of that stack 
  could be undefined after the thread terminates. Also you need to record somewhere the retval
  passed back by pthread_exit(), associated with that thread id, and make sure to not re-use the 
  same thread id again until a pthread_join call is made; that is, the thread is a zombie until 
  another thread does a pthread_join. 
  */

  threads[CurrentThread].status = TS_EXITED;

  if(retval != NULL) {
    threads[CurrentThread].retval = retval; 
  }

  schedule_wrap(0); 
  __builtin_unreachable(); 
}

pthread_t pthread_self(void){
  return threads[CurrentThread].id; 
}

int pthread_join(pthread_t thread, void **retval) {

  if(thread == pthread_self()) {
    fprintf(stderr, "ERROR: Thread cannot join with itself!\n"); 
    return EDEADLK; 
  }

  if(thread < 0 || (long int)thread >= MAX_THREADS) {
    fprintf(stderr, "ERROR: Invalid thread id: %d\n", (int)thread); 
    return EINVAL; 
  }

  int found = 0; 

  for(int i = 0; i < MAX_THREADS; i++){
    if(threads[i].status != TS_UNINITIALIZED && threads[i].id == thread){
      while(threads[i].status != TS_EXITED){
        schedule_wrap(0); 
      }

      if(retval != NULL){
        *retval = threads[i].retval; 
      }

      threads[i].status = TS_EXITED;
      
      if(threads[i].stack != NULL){
        free(threads[i].stack); 
        threads[i].stack = NULL; 
      }

      found = 1; 
      break; 
    }
  }

  if(found == 0) {
    return -1; 
  }

  return 0;
}

/* Assignment 5: Thread Synchronization */ 

// Helper functions

/*
We also recommend that you create new static void lock() and static void unlock()
functions. Your lock function should disable the timer that calls your schedule routine, and
unlock should re-enable the timer. You can use the sigprocmask function to this end (one
function using SIG_BLOCK, the other using SIG_UNBLOCK, with a mask on your alarm signal). 
Use these functions to prevent your scheduler from running when your threading library is 
internally in a critical section (users of your library will use barriers and mutexes for critical 
sections that are external to your library).
*/

struct sigaction block; 
struct sigaction unblock; 

static void lock() {
  sigemptyset(&block.sa_mask); 
  sigaddset(&block.sa_mask, SIGALRM); 
  sigprocmask(SIG_BLOCK, &block.sa_mask, &unblock.sa_mask); 
}

static void unlock() {
  sigemptyset(&unblock.sa_mask); 
  sigaddset(&unblock.sa_mask, SIGALRM); 
  sigprocmask(SIG_UNBLOCK, &unblock.sa_mask, NULL); 
}

  
/*void status_check(mutex_t* _mutex){
  printf("_mutex->count = %d\n", _mutex->params._count); 
  printf("_mutex->init = %d\n", _mutex->params._init); 
  printf("_mutex->lock = %d\n", _mutex->params._lock); 
  printf("_mutex->owner = %d\n\n\n", _mutex->params._owner); 
}*/


// MUTEX Functions

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr) { 
	lock();
	
	mutex_t _mut; 
	
	_mut.vars._init = 1; 
	_mut.vars._count = 0; 
	_mut.vars._lock = 0; 
	_mut.vars._owner = pthread_self();

	for(int i = 0; i < MAX_THREADS; i++) {
		_mut.vars.thread_queue[i] = -1; 
	}

	*mutex = _mut.mutex; 
	unlock(); 	
	return 0; 
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	lock();
	
	mutex_t _mut; 
	_mut.mutex = *mutex;


	_mut.vars._init = 0;
	for(int i = 0; i < MAX_THREADS; i++) {
		_mut.vars.thread_queue[i] = -1;
	}

	*mutex = _mut.mutex;
	unlock(); 
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
	lock(); 

	mutex_t _mut; 

	while(_mut.vars._lock == 1) {
		_mut.vars.thread_queue[_mut.vars._count] = (int)pthread_self();
		_mut.vars._count++;

		threads[(int)pthread_self()].status = TS_BLOCKED;
		schedule_wrap(0);
	}

	if(_mut.vars._lock == 0) {
		_mut.vars._lock = 1;
		_mut.vars._owner = (int)pthread_self(); 
	}
	
	*mutex = _mut.mutex; 
	
	return 0; 
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	lock(); 

	mutex_t _mut; 
	_mut.mutex = *mutex; 

	if(_mut.vars._owner == (int)pthread_self()) {
		_mut.vars._lock = 0; 

		for(int i = 0; i < _mut.vars._count; i++) {
			for(int j = 0; j < MAX_THREADS; j++) {
				if(_mut.vars.thread_queue[i] == (int)threads[j].id) {
					threads[j].status = TS_READY;
				}
			}
		}

		*mutex = _mut.mutex; 
	}
	unlock(); 
	return 0; 
}



// Barrier functions

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count) {

  if(count == 0) {
    fprintf(stderr, "ERROR: count cant be 0!\n");
    unlock(); 
    return EINVAL;
  }

  lock(); 
  
  barrier_t _barrier;  

  if(sizeof(my_barrier) > sizeof(pthread_barrier_t)) {
    fprintf(stderr, "ERROR: my_barrier: %d, pthread_barrier_t: %d\n", (int)sizeof(my_barrier), (int)sizeof(pthread_barrier_t));
    unlock(); 
    return -1; 
  }

  _barrier.vars.init = 1;
  _barrier.vars.count = 0;
  _barrier.vars.limit = count; 
  _barrier.vars.head = NULL; 

  *barrier = _barrier.barrier; 
  unlock(); 
  
  return 0; 
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
	lock(); 

	barrier_t _barrier; 
	_barrier.barrier = *barrier;

  if(_barrier.vars.init != 1) {
    fprintf(stderr, "ERROR: This barrier isnt initialized!\n");
    unlock(); 
    return -1; 
  }

  struct node_t *current = _barrier.vars.head; 
  while(current != NULL) {
    struct node_t* next = current->next; 
    free(current); 
    current = next; 
  }

  _barrier.vars.head = NULL; 
	_barrier.vars.count = 0; 
	_barrier.vars.limit = 0;
	_barrier.vars.init = 0; 

	*barrier = _barrier.barrier; 
	unlock(); 
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
	lock(); 
	barrier_t _barrier; 
	_barrier.barrier = *barrier; 

  if(_barrier.vars.init != 1) {
    fprintf(stderr, "ERROR: This barrier isnt initialized!\n");
    unlock(); 
    return EINVAL; 
  }

  struct node_t* new_node = (struct node_t*)malloc(sizeof(struct node_t)); 
  if(!new_node) {
    fprintf(stderr, "ERROR: Failure to allocate memory for node!\n");
    unlock(); 
    return ENOMEM;
  }

  new_node->id = pthread_self(); 
  new_node->next = NULL;

  if(_barrier.vars.head == NULL) {
    _barrier.vars.head = new_node; 
  }
  else {
    struct node_t* current = _barrier.vars.head; 
    while(current->next != NULL) {
      current = current->next; 
    }
    current->next = new_node; 
  }

  _barrier.vars.count++;

	if(_barrier.vars.count < _barrier.vars.limit){
		threads[(int)pthread_self()].status = TS_BLOCKED; 

		*barrier = _barrier.barrier; 

		unlock();
		schedule_wrap(0);
		return 0; 
	}

	else{
		struct node_t* current = _barrier.vars.head; 

    while(current != NULL) {
      int tid = (int)current->id;
      threads[tid].status = TS_READY; 

      struct node_t* to_free = current; 
      current = current->next; 

      free(to_free); 
    }

    _barrier.vars.head = NULL; 

		_barrier.vars.count = 0; 
		*barrier = _barrier.barrier;
		unlock();
		return PTHREAD_BARRIER_SERIAL_THREAD;
	} 
}
