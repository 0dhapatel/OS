#include <assert.h>
#include "queue.h"

//generic enqueue
#ifdef _ADVANCED_QUEUE_
void efnqueue(node * rear, node* head, node * new_node)
{
  if (rear == NULL)
  {
    rear = new_node;
    head = new_node;
  }
  else
  {
    rear->tcb.next = new_node;
    rear = new_node;
  }
  return;
}

node* debbaqueue(node * rear, node* head)
{
  node * temp = head;
  if (head == NULL)
  {
    return temp;

  }else if ( head->tcb.next == NULL)
  {
    head = NULL;
    rear = NULL;
  }else{
    head = head->tcb.next;
  }
  return temp;
}
#endif //_ADVANCED_QUEUE_

void enqueue(my_pthread_tcb ** cur, my_pthread_tcb ** prev,  my_pthread_tcb * new_tcb)
{
  if (*cur == NULL)
  {
    //cur =(my_pthread_tcb*)malloc(sizeof(my_pthread_tcb));
   *cur = new_tcb;
   *prev = new_tcb;
   (*cur)->next = (*cur);
   return;
  }
  new_tcb->next = *cur;
  (*prev)->next = new_tcb;
  *prev = new_tcb;/*
  new_tcb->next = (*cur)->next;
  (*cur)->next = new_tcb;*/
  assert(*cur!=NULL);
  return;
}


void dequeue(my_pthread_tcb ** prev, my_pthread_tcb ** cur)
{
  my_pthread_tcb * temp = *cur;
  //node * cur = head;
  if (*cur == NULL)
  {
    return;
    
  }else if ( (*cur)->next == *cur) //last tcb left
  {
    free(temp);
    *cur  = NULL;
    *prev = NULL;
  }else{//general case
    (*prev)->next = (*cur)->next;
    *cur = (*cur)->next;
    free(temp);
  }
}


/*
 * routine that searches for tcb with tid in tcb list
 * returns 0 on failure and 1 on success
 */

int inQueue(my_pthread_tcb* currentTCB, my_pthread_t tid, uint numNodes)
{
my_pthread_tcb * temp = currentTCB;
int ii = 0;

 while( ii != numNodes)
  {
    if (temp->tid == tid)
    {
      return 1;
    }
    temp = temp->next;
    ii++;
  }
  return 0;
}

