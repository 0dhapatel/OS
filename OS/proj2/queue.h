#ifndef QUEUE_H_
#define QUEUE_H_

#ifndef MY_PTHREAD_H_
//#define MY_PTHREAD_H_
#include "my_pthread.h"
#endif //MY_PTHREAD_H_

void enqueue(my_pthread_tcb ** cur, my_pthread_tcb ** prev, my_pthread_tcb * new_node);
void dequeue(my_pthread_tcb ** cur, my_pthread_tcb ** prev);
int inQueue(my_pthread_tcb * cur, my_pthread_t num1, uint num2);
#endif //QUEUE_H_
