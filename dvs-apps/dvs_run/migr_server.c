#include "dvs_run.h"

#define  MIGR_TEST 	16077022
#define  MAXRETRIES	10

int loops, maxbuf;
message *m_ptr;
char *buffer;

double dwalltime()
{
	double sec;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	sec = tv.tv_sec + tv.tv_usec/1000000.0;
	return sec;
}
   
int  main ( int argc, char *argv[] )
{
	int  svr_pid, ret, i, svr_ep, retry, rcode ;
	double t_start, t_stop, t_total, loopbysec, tput; 

	/*---------------- SERVER binding ---------------*/
		
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	retry = 0;
	do {
		retry++;
		USRDEBUG("[%s] retry=%d\n", argv[0], retry);
		rcode = dvk_wait4bind_T(1000);
		if( retry == MAXRETRIES) ERROR_EXIT(EDVSNOTBIND);
	}while(rcode < EDVSERRCODE);
	svr_pid = getpid();
	svr_ep = rcode;
   	USRDEBUG("SERVER svr_pid=%d svr_ep=%d\n", svr_pid, svr_ep);

	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		fprintf(stderr, "SERVER: message posix_memalign errno=%d\n", errno);
   		exit(1);
	}
	
	/*---------------- Allocate memory for DATA BUFFER ---------------*/
	posix_memalign( (void**) &buffer, getpagesize(), MAXCOPYLEN);
	if (buffer== NULL) {
   		fprintf(stderr, "SERVER buffer posix_memalign errno=%d\n", errno);
   		exit(1);
  	}
	
	/*---------------- Fill with characters the DATA BUFFER ---------------*/
	srandom( getpid());
	for(i = 0; i < MAXCOPYLEN-2; i++){
#define MAX_ALPHABET ('z' - '0')
#if RANDOM_PATERN
		buffer[i] =  (random()/(RAND_MAX/MAX_ALPHABET)) + '0';
#else	
		buffer[i] = ((i%25) + 'a');	
#endif
	}
	buffer[MAXCOPYLEN-1] = 0;
	buffer[30] = 0;	
	USRDEBUG("SERVER buffer %s\n", buffer);
		
	/*---------------------- M3-IPC SERVER LOOP ----------------------------  */
 	USRDEBUG("SERVER: Starting loops\n");
	loops = 0;
	while(TRUE) {
		USRDEBUG("SERVER: Receiving loops=%d\n", loops);
		ret = dvk_receive_T(ANY, (long) m_ptr, TIMEOUT_MOLCALL);
		if( ret == EDVSTIMEDOUT) continue;

		if( ret < 0) ERROR_EXIT(ret);
		USRDEBUG("SERVER RECEIVE:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		
		if( m_ptr->m_type != MIGR_TEST){
		   	fprintf(stderr, "SERVER: Message type not MIGR_TEST (%d)\n", m_ptr->m_type);	
		}
		
		ret = dvk_vcopy(svr_ep, buffer, m_ptr->m_source, m_ptr->m1_p1, m_ptr->m1_i1);	
		if(ret < 0) {
			USRDEBUG("SERVER: VCOPY error=%d\n", ret);
			exit(1);
		}

		m_ptr->m_type = ret;	
		m_ptr->m1_i3  = loops;
		USRDEBUG("SERVER SEND:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		ret = dvk_send_T(m_ptr->m_source, (long) m_ptr, TIMEOUT_MOLCALL);
		if( ret == EDVSTIMEDOUT) {
	   		fprintf(stderr, "SERVER send timeout loops=%d\n", loops);
		}
		loops++;		
	}
	
	/*--------- Report statistics  ---------*/
	t_total = (t_stop-t_start);
	loopbysec = (double)(loops)/t_total;
	tput = loopbysec * (double)maxbuf;
 	printf("t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
 	printf("transfer size=%d #transfers=%d loopbysec=%f\n", maxbuf , loops, loopbysec);
 	printf("Throuhput = %f [bytes/s]\n", tput);
	USRDEBUG("SERVER END\n");
}



