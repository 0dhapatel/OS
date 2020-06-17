CC = gcc
CFLAGS = -g

all: ThreadRun ThreadJoin MultiThreadRun MultiThreadJoin MultiThreadYield 

ThreadRun: ThreadRun.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) ThreadRun.c my_pthread.c queue.c -o ThreadRun

ThreadJoin: ThreadJoin.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) ThreadJoin.c my_pthread.c queue.c -o ThreadJoin

MultiThreadRun: MultiThreadRun.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) MultiThreadRun.c my_pthread.c queue.c -o MultiThreadRun

MultiThreadJoin: MultiThreadJoin.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) MultiThreadJoin.c my_pthread.c queue.c -o MultiThreadJoin

MultiThreadYield: MultiThreadYield.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) MultiThreadYield.c my_pthread.c queue.c -o MultiThreadYield

#pthread: my_pthread.c my_pthread.h 
	#$(CC) $(CFLAGS) my_pthread.c queue.c  -o my_pthread

#Queue: queue.c queue.h
	#$(CC) $(CFLAGS) queue.c  -o queue

clean:
	rm -f ThreadRun TheadJoin MultiThreadRun MultiThreadJoin MultiThreadYield 
