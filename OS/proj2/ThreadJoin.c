#include <stdio.h>
#include <stdlib.h>
#include "my_pthread.h"

void thread_run(){
  int x;
  for(x = 200; x > 0 ; x--){
    printf("Thread %d Running, Counter at %d\n", my_pthread_self(), x);
  }
  for(x = 200; x > 0 ; x--){
  	my_pthread_yield();
    printf("Thread %d Yield, Counter at %d\n", my_pthread_self(), x);
  }
  printf("Thread %d Finished\n",  my_pthread_self());
  my_pthread_exit();
}


int main(){
  int x;

  my_pthread_t thread;

  my_pthread_create(&thread, (void*)thread_run, (void*)NULL);
  
  for(x = 200; x > 0 ; x--){
  	my_pthread_yield();
    printf("Thread %d Yield out, Counter at %d\n", my_pthread_self(), x);
  }

  my_pthread_join(thread);

  printf("Main Thread Resuming\n");

  printf("Main Thread Exiting\n");
}
