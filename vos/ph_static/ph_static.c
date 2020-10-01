#include <stdio.h>
#include <pthread.h>

pthread_t pth_nr;

static void pth_test(void *var)
{
	while(1){
		printf("pth_test\n");
		sleep(2);
	}
}

int main ( int argc, char *argv[] )
{
	int rcode;
	rcode = pthread_create( &pth_nr, NULL, pth_test, NULL);
	if(rcode)
		printf("ERROR rcode=%d\n",rcode);;
	printf("pth_nr=%u\n",pth_nr);
	
	pthread_join(pth_nr, &rcode);
	printf("rcode=%d\n",rcode);
}
