#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#define _POSIX_C_SOURCE 199309
#include <time.h>


#define MAXLOOP 100000L
#define USO_CPU 10L
#define NSECS  	2000000L

 
int main (int argc, char *argv[])
{
  long int i, j, s;
  struct timespec request, remain; 
  long   maxloop, uso_cpu, nsecs;  

	if( argc == 1){
		maxloop 	= MAXLOOP;
		uso_cpu 	= USO_CPU;
		nsecs		= NSECS;
	} else if (argc != 4) {
		printf("usage: gradomp [<uso_cpu> <maxloop> <nsecs>]\n");
		printf("example: uso_cpu=10 maxloop=100000 nsecs=2000000\n");
		exit(1);
	} else {
		uso_cpu 	= atol(argv[1]);
		maxloop 	= atol(argv[2]);
		nsecs		= atol(argv[3]);		
	}

	printf("gradomp: uso_cpu=%ld maxloop=%ld nsecs=%ld\n",
		uso_cpu, maxloop, nsecs);

  while(1)
    {
//    printf("CPU ");
/*    fflush(0); */
    for ( i = 0; i < uso_cpu; i++)
        for(j = 0; j < maxloop; j++);

    request.tv_sec = 0L;
    request.tv_nsec = nsecs;
//    printf("E/S ");
/*    fflush(0); */
    s = nanosleep(&request, &remain);
    }
}
