CC = gcc
CFLAGS = -g
link = -lpthread 
all: ThreadRun ThreadJoin MultiThreadRun MultiThreadJoin MultiThreadYield

ThreadRun: ThreadRun.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) ThreadRun.c my_pthread.c queue.c -o ThreadRun $(link)

ThreadJoin: ThreadJoin.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) ThreadJoin.c my_pthread.c  queue.c -o ThreadJoin $(link)

MultiThreadRun: MultiThreadRun.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) MultiThreadRun.c my_pthread.c queue.c  -o MultiThreadRun $(link)

MultiThreadJoin: MultiThreadJoin.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) MultiThreadJoin.c my_pthread.c  queue.c  -o MultiThreadJoin $(link)

MultiThreadYield: MultiThreadYield.c my_pthread.h my_pthread.c queue.h queue.c
	$(CC) $(CFLAGS) MultiThreadYield.c my_pthread.c  queue.c -o MultiThreadYield $(link)

pthread: my_pthread.c my_pthread.h queue.h queue.c
	$(CC) $(CFLAGS) my_pthread.c queue.c  -o my_pthread

Queue: queue.c queue.h
	$(CC) $(CFLAGS) queue.c  -o queue

clean:
	rm -f ThreadRun TheadJoin MultiThreadRun MultiThreadJoin MultiThreadYield
