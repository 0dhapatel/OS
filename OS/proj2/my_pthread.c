#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>

#include "my_pthread.h"
#include "queue.h"

/* Macros Here*/
#define STACK_SIZE SIGSTKSZ
 
/* Scheduler State */
 // Fill in Here //

//* lock for queue *//
pthread_mutex_t  gQueueLock;

//* lock for thread id counter *//
pthread_mutex_t  gCounterLock;

//* lock for signal handler *//
pthread_mutex_t  gSignalLock;

//* scheduler context pointer *//
ucontext_t * sctx;

//* schecduler return pointer *//
ucontext_t * srctx;

//* thread id counter *//
uint gTidCnt = 0;

//* total thread counter *//
uint gTotalThreads = 0;

//* signal handler struct *//
struct sigaction sa;

//* timer struct
struct itimerval timer;

//* current context pointer *//
my_pthread_tcb * gCurrentThread = NULL;

//* previous context pointer *//
my_pthread_tcb * gPreviousThread = NULL;

//* flags *//

int gThreadExit = 0;

#ifdef _ADVANCED_QUEUE_
//* rear ready queue pointer *//
my_pthread_tcb * gReadyQR;

//* head ready queue pointer *//
my_pthread_tcb * gReadyQR;
#endif //_ADVANCED_QUEUE_

void printQueue(int);

/* Scheduler Function
 * Pick the next runnable thread and swap contexts to start executing
 */
void schedule(int signum){

  // Implement Here
  //printf("previous Thread %d\n",gCurrentThread->tid);
  if((pthread_mutex_trylock(&gSignalLock))!=0)
  {
    //swapcontext(&gCurrentThread->context,&gPreviousThread->context);
    return;
  }
  else if(gThreadExit)
  {
    //* dont need to save the current context, go to scheduler
    pthread_mutex_unlock(&gSignalLock);
    setcontext(sctx);
  }
  else
  {
    //* save context of the current running task and switch to scheduler
    pthread_mutex_unlock(&gSignalLock);
    swapcontext(&gCurrentThread->context,sctx);
  }
}

/*
 * routine we initialize the scheduler context with
 * handles scheduling of next task
 */
void scheduler(void)
{
  //just dequeued from scheduler
  if (gThreadExit)
  {
    pthread_mutex_lock(&gSignalLock);
    sa.sa_handler = &schedule;
    sigaction(SIGPROF,&sa,NULL);
    pthread_mutex_unlock(&gSignalLock);
    gThreadExit = 0;
    assert(gCurrentThread!=NULL);
    setcontext(&gCurrentThread->context);
    return;
  }
  
    
  //result of pthread_create
  gPreviousThread = gCurrentThread;
  gCurrentThread = gCurrentThread->next;
  
  //reinstate signal handler
  pthread_mutex_lock(&gSignalLock);
  sa.sa_handler = &schedule;
  sigaction(SIGPROF,&sa,NULL);
  pthread_mutex_unlock(&gSignalLock);

  //* dont care about saving the current context
  //* set context to next thread in queue
  assert(gCurrentThread!=NULL);
  setcontext(&gCurrentThread->context);
    
  return;
}


/*
 * initialize context pointed to by context with function func
 */
void context_init(ucontext_t * context, void* (*func)(void*))
{
  void * stack; 
  //* grad current context
  getcontext(context);
  context->uc_link = 0;
  if ((stack = malloc(STACK_SIZE)) == NULL)
  {
    perror("Failed to allocate stack");
    exit(1);
  }
  context->uc_stack.ss_sp = stack;
  context->uc_stack.ss_size = STACK_SIZE;
  context->uc_stack.ss_flags = 0;
  makecontext(context,(void*)func,0);
}

/*
 * initializes everything needed for the scheduler
 * this includes the timer, invoking the signal handler 
 * for SIGPROF signal, grabbing the main context and of
 * course  initializing the sctx context
 */
void scheduler_init()
{
  //* main process context
  ucontext_t cctx;
  
  //* initialize locks
  pthread_mutex_init(&gQueueLock,NULL);
  pthread_mutex_init(&gSignalLock,NULL);
  pthread_mutex_init(&gCounterLock,NULL);

  //* cautionary setting of globals to null
  gPreviousThread = NULL;
  gCurrentThread = NULL;

  //* thread control block for main context
  my_pthread_tcb * tcb = (my_pthread_tcb*)malloc(sizeof(my_pthread_tcb));
  
  if (tcb == NULL)
  {
    perror("Failed to allocate tcb block");
  }

  //* allocate scheduler context
  sctx = malloc(sizeof(my_pthread_tcb));
  
  //get current context
  getcontext(sctx);
  sctx->uc_link = 0;
  sctx->uc_stack.ss_sp = malloc(STACK_SIZE);
  sctx->uc_stack.ss_size = STACK_SIZE;
  sctx->uc_stack.ss_flags = 0;

  //create scheduler context
  makecontext(sctx,(void*)&scheduler,0);
 
  //clear timer struct 
  memset(&timer, 0, sizeof(struct itimerval));

  //configure initial timer expiration time
  timer.it_value.tv_sec=0;
  timer.it_value.tv_usec=TIME_QUANTUM_MS;
  
  //have it repeat at TIME_QUANTUM_MS rate
  timer.it_interval.tv_sec=0;
  timer.it_interval.tv_usec=TIME_QUANTUM_MS;
  
  //set interval timer
  setitimer(ITIMER_PROF, &timer, NULL);

  //* set tid and status
  //* status currentlu not used
  tcb->tid = gTidCnt++;
  tcb->status = RUNNABLE;
  
  //* add the main context to the queue
  //* lock first tho :P
  pthread_mutex_lock(&gQueueLock);
  enqueue(&gCurrentThread,&gPreviousThread,tcb);
  gTotalThreads++;
  assert(gCurrentThread!=NULL);
  assert(gPreviousThread!=NULL);  
  pthread_mutex_unlock(&gQueueLock);

  //* initialize the signal handler
  //* point it to scheduler
  sa.sa_handler = &schedule; 
  sigaction(SIGPROF,&sa, NULL);
}

/* Create a new TCB for a new thread execution context and add it to the queue
 * of runnable threads. If this is the first time a thread is created, also
 * create a TCB for the main thread as well as initalize any scheduler state.
 */

void my_pthread_create(my_pthread_t *thread, void*(*function)(void*), void *arg){

  // Implement Here
  
  //* current context
  ucontext_t cctx;

  my_pthread_tcb * tcb = (my_pthread_tcb*)malloc(sizeof(my_pthread_tcb));

  //* cautionary clearing of context
  memset(&cctx,0,sizeof(ucontext_t));
  
  //* first time being called
  if (sctx == NULL)
  {
    scheduler_init();
  }

  //* initialize current context
  context_init(&cctx,(void*)function);

  pthread_mutex_lock(&gCounterLock);
  tcb->tid  = *thread = gTidCnt++;
  pthread_mutex_unlock(&gCounterLock);

  tcb->status = RUNNABLE;
  tcb->context = cctx;

  //* add new context to the queue
  //* lock the queue tho :P
  pthread_mutex_lock(&gQueueLock);
  assert(gCurrentThread!=NULL);
  assert(gPreviousThread!=NULL);    
  enqueue(&gCurrentThread,&gPreviousThread, tcb);
  gTotalThreads++;
  //printQueue(gTotalThreads);
  pthread_mutex_unlock(&gQueueLock);

}

/* Give up the CPU and allow the next thread to run.
 */
void my_pthread_yield(){
  
  //* pthread_create has not been called yet
  if (sctx == NULL)
  {
    perror("cannot yield before calling pthread_create");
  }

  //* kill signal handler
  pthread_mutex_lock(&gSignalLock);
  sa.sa_handler = SIG_DFL;
  sigaction(SIGPROF,&sa,NULL);
  pthread_mutex_unlock(&gSignalLock);
  schedule(-1);

}

/* The calling thread will not continue until the thread with tid thread
 * has finished executing.
 */
void my_pthread_join(my_pthread_t thread){

  // Implement Here //
  //* pthread_create has not been called yet
  if (sctx == NULL)
  {
    perror("cannot join before calling pthread_create");
    exit(1);
  }

  //* thread is still in the queue
  //* lock queue while looking through it
  pthread_mutex_lock(&gQueueLock);
  while(inQueue(gCurrentThread,thread,gTotalThreads) == 1)
  {
    pthread_mutex_unlock(&gQueueLock);
    //* yield the CPU
    my_pthread_yield();
    //* lock queue while looking through it
    pthread_mutex_lock(&gQueueLock);
  }
  pthread_mutex_unlock(&gQueueLock);
 
}


/* Returns the thread id of the currently running thread
 */
my_pthread_t my_pthread_self(){

  // Implement Here //
  //* pthread_create has not been called yet
  if (sctx == NULL)
  {
    perror("cannot return thread id before calling pthread_create");
  }

  return gCurrentThread->tid;

}

/* Thread exits, setting the state to finished and allowing the next thread
 * to run.
 */
void my_pthread_exit(){

  // Implement Here //

  //* pthread_create has not been called yet
  if (sctx == NULL)
  {
    perror("cannot exit before calling pthread_create");
    exit(1);
  }

  //* remove current context from queue
  //* but first lock the queue :P
  pthread_mutex_lock(&gQueueLock);
  assert(gCurrentThread!=NULL);
  assert(gPreviousThread!=NULL);  
  dequeue(&gPreviousThread, &gCurrentThread);
  gTotalThreads--;
  pthread_mutex_unlock(&gQueueLock);
  
  //* kill signal handler
  pthread_mutex_lock(&gSignalLock);
  sa.sa_handler = SIG_DFL;
  sigaction(SIGPROF,&sa,NULL);
  pthread_mutex_unlock(&gSignalLock);

  //* flip exit flag and call scheduler
  gThreadExit = 1;
  schedule(-1);

}

void printQueue(int numNodes)
{
  int ii;
  my_pthread_tcb * temp = gCurrentThread;
  while( ii != numNodes)
    {
      printf("%d(%d)->",temp->tid,temp->next->tid);
      temp = temp->next;
      ii++;
      
    }
  printf("\n");

}

void nothing(void)
{
  while(1){}
}

