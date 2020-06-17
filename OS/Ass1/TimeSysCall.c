#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

int main(int argc, char *argv[]){
	
	struct timeval start, end;
	double total_time=0;
	int i=0; 
		gettimeofday(&start, NULL);
	for(i=0;i<100000;i++){
		//gettimeofday(&start, NULL);
		getpid();
		//gettimeofday(&end, NULL);
		//total_time+= ((end.tv_usec) - (start.tv_usec));
	}
		gettimeofday(&end, NULL);
		total_time= 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec) - (start.tv_usec);
	double average = total_time/100000;
	printf("Syscalls Performed: %d\n", 100000);
	printf("Total Elapsed Time: %lf microseconds\n", total_time);
	printf("Average Time Per Syscall: %lf microseconds\n", average);
	
	return 0;

}
