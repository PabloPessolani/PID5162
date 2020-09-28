#include "dvs_run.h"

#define   MIGR_TEST 	16077022
#define   MAXLOOPS 		10000
#define   MAXDELAY 		(5*60)
#define  MAXRETRIES		10

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

void print_usage(char *argv0){
fprintf(stderr,"Usage: %s <svr_ep> <maxbuf> <loops> <delay>\n", argv0 );
	fprintf(stderr,"<svr_ep>: server endpoint (%d-%d)\n", (-NR_TASKS), (NR_PROCS-NR_TASKS));
	fprintf(stderr,"<maxbuf>: Maximum buffer size (1-%d)\n", MAXCOPYLEN);
	fprintf(stderr,"<loop>:  number of client/server loops (1-%d)\n", MAXLOOPS);
	fprintf(stderr,"<delay>: delay in seconds between requests (0-%d)\n", MAXDELAY);
	ERROR_EXIT(EDVSINVAL);	
}
   
int  main ( int argc, char *argv[] )
{
	int  clt_pid, ret, i, clt_ep, svr_ep, delay, rcode, retry;
	double t_start, t_stop, t_total, loopbysec, tput; 
	long int loops, total_bytes = 0 ;
	proc_usr_t *proc_ptr;

	USRDEBUG("[%s] argc=%d\n", argv[0], argc);
	for( i = 1; i < argc ; i++) {
		USRDEBUG("[%s] argv[%d]=%s\n", argv[0], i, argv[i]);
	}
	
	if( argc != 5) {
		print_usage(argv[0]);
	}

	svr_ep = atoi(argv[1]);
	maxbuf = atoi(argv[2]);
	loops  = atoi(argv[3]);
	delay  = atoi(argv[4]);
	
	if( (svr_ep < (-NR_TASKS)) || (svr_ep > (NR_PROCS-NR_TASKS))){
		fprintf(stderr,"Bad server endpoint (%d)\n", svr_ep);
		ERROR_EXIT(EDVSENDPOINT);
	}
	
	if( (maxbuf <= 0) || (maxbuf > MAXCOPYLEN)){
		fprintf(stderr,"Bad Buffer size (%d)\n", maxbuf);
		ERROR_EXIT(EDVSBADVALUE);
	}
	
	if( (loops <= 0) || (loops > MAXLOOPS)){
		fprintf(stderr,"Bad loops number (%d)\n", loops);
		ERROR_EXIT(EDVSBADVALUE);
	}

	if( (delay < 0) || (loops > MAXDELAY)){
		fprintf(stderr,"Bad delay value (%d)\n", delay);
		ERROR_EXIT(EDVSBADVALUE);
	}
	
	/*---------------- CLIENT binding ---------------*/
		
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	retry = 0;
	do {
		retry++;
		USRDEBUG("[%s] retry=%d\n", argv[0], retry);
		rcode = dvk_wait4bind_T(TIMEOUT_MOLCALL);
		USRDEBUG("[%s] rcode=%d\n", argv[0], rcode);
		if( rcode == EDVSTIMEDOUT) {
			if( retry == MAXRETRIES) 
				ERROR_EXIT(EDVSNOTBIND);
		} else {
			if(rcode < EDVSERRCODE) 
				ERROR_EXIT(rcode);
		}
	}while(rcode == EDVSTIMEDOUT);
	
	clt_pid = getpid();
	clt_ep = rcode;
   	USRDEBUG("CLIENT clt_pid=%d clt_ep=%d\n", clt_pid, clt_ep);

	/*---------------- Allocate memory for process descriptor  ---------------*/
	posix_memalign( (void **) &proc_ptr, getpagesize(), sizeof(proc_usr_t) );
	if (proc_ptr== NULL) {
   		fprintf(stderr, "CLIENT: proc_usr_t posix_memalign errno=%d\n", errno);
   		exit(1);
	}
	
	ret = dvk_getprocinfo(PROC_NO_PID, _ENDPOINT_P(svr_ep), proc_ptr);
	if( ret) ERROR_EXIT(ret);
	USRDEBUG("CLIENT: " PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr)); 
	
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		fprintf(stderr, "CLIENT: message posix_memalign errno=%d\n", errno);
   		exit(1);
	}
	
	/*---------------- Allocate memory for DATA BUFFER ---------------*/
	posix_memalign( (void**) &buffer, getpagesize(), MAXCOPYLEN);
	if (buffer== NULL) {
   		fprintf(stderr, "CLIENT buffer posix_memalign errno=%d\n", errno);
   		exit(1);
  	}

	/*---------------------- M3-IPC CLIENT LOOP ----------------------------  */
 	USRDEBUG("CLIENT: Starting loops\n");
	t_start = dwalltime();
	for( i = 0; i < loops; i++) {
		
		memset(buffer,'A',maxbuf-2);
		buffer[maxbuf-1] = 0;
		if( maxbuf > 30)
			buffer[30] = 0;		
		USRDEBUG("CLIENT buffer BEFORE [%s]\n", buffer);
		m_ptr->m_type = MIGR_TEST;
		m_ptr->m1_i1  = maxbuf;
		m_ptr->m1_i2  = i;
		m_ptr->m1_i3  = 0;
		m_ptr->m1_p1  = buffer;
		USRDEBUG("CLIENT SEND:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		ret = dvk_sendrec_T(svr_ep, (long) m_ptr, TIMEOUT_MOLCALL);
		if( ret == EDVSTIMEDOUT || ret == EDVSAGAIN) continue;
		if( ret < 0) ERROR_EXIT(ret);

		USRDEBUG("CLIENT RECEIVE:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));	
		if( m_ptr->m_type < 0){
		   	fprintf(stderr, "CLIENT: Server error (%d)\n", m_ptr->m_type);
			exit(0);
		}
		
		if( m_ptr->m_type > 30)
			buffer[30] = 0;	
		USRDEBUG("CLIENT buffer AFTER [%s]\n", buffer);
		
		if (delay > 0) sleep(delay);
	}
   	t_stop  = dwalltime();

	t_total = (t_stop-t_start);
	total_bytes = loops*maxbuf;
	loopbysec = (double)(loops)/t_total;
	tput = loopbysec * (double)maxbuf;	
	printf("t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
	printf("Requests message size=%d #transfers=%d loopbysec=%f\n", sizeof(message), loops, loopbysec);
	printf("Message Transfer Throuput=%f [msg/s]\n", loopbysec*2);
	printf("Data Transfer size=%d #transfers=%d loopbysec=%f\n", maxbuf, loops, loopbysec);
	printf("Throuhput = %f [bytes/s]\n", tput);
		
	USRDEBUG("CLIENT END\n");
}



