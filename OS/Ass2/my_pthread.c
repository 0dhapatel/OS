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

//* stack context pointer *//
ucontext_t * sctx;

//* thread id counter *//
uint gTidCnt = 0;

//* signal handler struct *//
struct sigaction sa;

//* current context pointer *//
my_pthread_tcb * gCurrentThread;

//* previous context pointer *//
my_pthread_tcb * gPreviousThread;

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
  ucontext_t nctx;
  //swapcontext current context with scheduler context
  swapcontext(&nctx,sctx);
  
}

void scheduler(void)
{
  printf("interrupt occured!\n");
  //set previous thread equal to current thread
  //gPreviousThread = gCurrentThread;
  //set current thread to the next thread
  //gCurrentThread = gCurrentThread->next;
  
  //reinstate signal handler
  sa.sa_handler = &schedule;
  sigaction(SIGPROF,&sa,NULL);
  
  //swap to next thread
  // swapcontext(&gPreviousThread->context, &gCurrentThread->context);
}

void context_init(ucontext_t * context, void* (*func)(void*))
{
  void * stack; 
  getcontext(context);
  context->uc_link = 0;
  if ((stack = malloc(STACK_SIZE)) == NULL)
  {
    perror("Failed to allocate stack");
    exit(1);
  }
  context->uc_stack.ss_sp	= stack;
  context->uc_stack.ss_size	= STACK_SIZE;
  context->uc_stack.ss_flags	= 0;
  makecontext(context,(void*)func,0);
}

void scheduler_init(void)
{
  //* main process context
  ucontext_t cctx;
  
  //* timer struct
  struct itimerval timer;
  
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
  
  //get information for the main thread
  //ignore context, it will be added in the scheduler
  //printf("in scheduler before: %d\n",gTidCnt);
  tcb->tid = gTidCnt++;
  //printf("in scheduler after: %d\n",gTidCnt);
  tcb->status = RUNNABLE;
  enqueue(gCurrentThread,tcb);
  //assert(gCurrentThread!=NULL);
  gCurrentThread = tcb;
  
  //this might run twice but idk a better way yet
  sa.sa_handler = &schedule; 
  sigaction(SIGPROF,&sa, NULL);
}

/* Create a new TCB for a new thread execution context and add it to the queue
 * of runnable threads. If this is the first time a thread is created, also
 * create a TCB for the main thread as well as initalize any scheduler state.
 */

void my_pthread_create(my_pthread_t *thread, void*(*function)(void*), void *arg){

  // Implement Here
  //* create three threads
  //* one for the calling function (my_pthread_create)
  //* one for the passed in function
  //* one fot the scheduler
  //* first time running
  if (sctx == NULL)
  {
    scheduler_init();
  }
  
  ucontext_t cctx;
  my_pthread_tcb * tcb = (my_pthread_tcb*)malloc(sizeof(my_pthread_tcb));
      
  //clear structs
  memset(&cctx,0,sizeof(ucontext_t));
  
  context_init(&cctx,(void*)function);
  //printf("in create a: %d\n",gTidCnt);
  tcb->tid = gTidCnt++;
  //printf("in create b: %d\n",gTidCnt);
  tcb->status = RUNNABLE;
  tcb->context = cctx;
  //remove later just for testing queue function
  pthread_mutex_lock(&gQueueLock);
  enqueue(gCurrentThread, tcb);
  pthread_mutex_unlock(&gQueueLock);
  

}

/* Give up the CPU and allow the next thread to run.
 */
void my_pthread_yield(){
 
  //* current context
  ucontext_t cctx;
  
  //*kill signal handler
  sa.sa_handler = SIG_DFL;
  sigaction(SIGPROF,&sa,NULL);
  
  //get context of current function
  getcontext(&cctx);
  
  //swap to scheduler
  swapcontext(&cctx,sctx);
}

/* The calling thread will not continue until the thread with tid thread
 * has finished executing.
 */
void my_pthread_join(my_pthread_t thread){

  // Implement Here //
  while(inQueue(gCurrentThread,thread,gTidCnt) == 1)
  {
    my_pthread_yield();
  }
 
}


/* Returns the thread id of the currently running thread
 */
my_pthread_t my_pthread_self(){

  // Implement Here //
  return gCurrentThread->tid;
  //return 0; // temporary return, replace this

}

/* Thread exits, setting the state to finished and allowing the next thread
 * to run.
 */
void my_pthread_exit(){

  // Implement Here //
  //* lock queue
  pthread_mutex_lock(&gQueueLock);
  dequeue(gPreviousThread, gCurrentThread);
  pthread_mutex_lock(&gQueueLock);
  
  //turn off signal handling
  sa.sa_handler = SIG_DFL;
  sigaction(SIGPROF,&sa,NULL);
  //call scheduler
  schedule(0);
  //* dequeue
  //* unlock queue //
  //* turn off signal handling
  //* scheduler
}

/*void printQueue(int numNodes)
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

void nothing(void){

}

int main(int argc,char** argv)
{
  int ii;
  my_pthread_t thread;
  my_pthread_create(&thread,(void*)nothing,NULL);
  my_pthread_create(&thread,(void*)nothing,NULL);
  my_pthread_create(&thread,(void*)nothing,NULL);
  my_pthread_create(&thread,(void*)nothing,NULL);
  my_pthread_create(&thread,(void*)nothing,NULL);
  printQueue(8);
  return 0;
}*/
