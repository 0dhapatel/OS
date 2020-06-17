#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

int i=0,z=0,x=7,y=0;
struct timeval start, end;
double total_time=0;

void handle_sigfpe(int signum){

	i++;
	if(i==100000){
		gettimeofday(&end, NULL);
		total_time= 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec) - (start.tv_usec);
		double average = total_time/100000;
		printf("Exceptions Occurred: %d\n", 100000);
		printf("Total Elapsed Time: %lf microseconds\n", total_time);
		printf("Average Time Per Syscall: %lf microseconds\n", average);
		exit(0);
	}
}

int main(int argc, char *argv[]){

    	
	gettimeofday(&start, NULL);
	signal(SIGFPE, handle_sigfpe);
    	z = x / y;
	//gettimeofday(&end, NULL);
	return 0;

}
