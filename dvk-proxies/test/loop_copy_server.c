#include "loops.h"

#define		RANDOM_PATERN	1
#define 	RMTNODE	1
#define 	DCID	0
#define 	SVR_NR	0
#define 	CLT_NR	(SVR_NR + 1)
#define 	MAXCHILDREN	100

int dcid, loops, children, maxbuf;
message *m_ptr;
char *buffer;

int child_function(int child) {
	int i , ret;
	pid_t child_pid;
	int child_nr,client_nr, child_ep;
	char rmt_name[16];
	
  	child_nr = SVR_NR + ((child+1) * 2);
	USRDEBUG("CHILD child_nr %d: loops=%d child=%d\n", child_nr, loops, child);
		
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		fprintf(stderr, "CHILD child_nr %d: message posix_memalign\n", child_nr);
   		exit(1);
	}
	USRDEBUG("CHILD child %d: m_ptr=%p\n",child_nr, m_ptr);
	
	/*---------------- Allocate memory for DATA BUFFER ---------------*/
	posix_memalign( (void**) &buffer, getpagesize(), MAXCOPYLEN);
	if (buffer== NULL) {
   		fprintf(stderr, "CHILD child_nr %d:buffer posix_memalign\n", child_nr);
   		exit(1);
  	}
	
	/*---------------- Fill with EDVSacters the DATA BUFFER ---------------*/
	srandom( getpid());
	
	for(i = 0; i < maxbuf-1; i++){
#define MAX_ALPHABET ('z' - '0')
#if RANDOM_PATERN
		buffer[i] =  (random()/(RAND_MAX/MAX_ALPHABET)) + '0';
#else	
		buffer[i] = ((i%25) + 'a');	
#endif
	}
	buffer[maxbuf] = 0;
	
	if( maxbuf > 30) buffer[30] = 0;	
	USRDEBUG("CHILD child %d: buffer before=%s\n", child, buffer);
		
	/* binding remote client */
	client_nr = child_nr + 1;
	sprintf(rmt_name,"client%d",client_nr);
	ret = dvk_rmtbind(dcid, rmt_name, client_nr, RMTNODE);
	if(ret < 0) {
		fprintf(stderr,"CHILD child %d: dvk_rmtbind process %s on node %d \n", 
			client_nr , rmt_name, RMTNODE);
		exit(1);		
	}

	/* binding local server */
	child_ep = dvk_bind(dcid, child_nr);
	child_pid = getpid();
	USRDEBUG("CHILD child_nr=%d: child_ep=%d child_pid=%d\n", 
		child_nr, child_ep, child_pid);
	
	/* synchronization with MAIN SERVER */
	ret = dvk_sendrec(SVR_NR, (long) m_ptr);
	if(ret < 0) {
		fprintf(stderr,"CHILD child_nr %d: dvk_sendrec ret=%d\n", child_nr, ret);
		exit(1);		
	}
		
	/* wait for message from remote client */
	ret = dvk_receive(client_nr, (long) m_ptr);
 	USRDEBUG("CHILD %d: " MSG1_FORMAT, child, MSG1_FIELDS(m_ptr));
	if(ret < 0) {
		fprintf(stderr,"CHILD child_nr %d: dvk_receive ret=%d\n", child_nr, ret);
		exit(1);		
	}	
		
	/* M3-IPC TRANSFER LOOP  */
 	USRDEBUG("CHILD child_nr %d:Starting loop\n", child_nr);
	for( i = 0; i < loops; i++) {
		ret = dvk_vcopy(child_ep, buffer, client_nr, m_ptr->m1_p1, maxbuf);	
		if(ret < 0) {
			USRDEBUG("CHILD child_nr %d: VCOPY error=%d\n", child_nr, ret);
			exit(1);
		}
	}

	/* reply to remote client */
	ret = dvk_send(client_nr, (long) m_ptr);
	if(ret < 0) {
		fprintf(stderr,"CHILD child_nr %d: dvk_send ret=%d\n", child_nr, ret);
		exit(1);		
	}	

	/* STOP synchronization with MAIN SERVER */
	ret = dvk_sendrec(SVR_NR, (long) m_ptr);
	if(ret < 0) {
		fprintf(stderr,"CHILD child_nr %d: dvk_sendrec ret=%d\n", child_nr, ret);
		exit(1);		
	}	
	
	USRDEBUG("CHILD child_nr %d: unbinding %d\n", child_nr,client_nr);
	dvk_unbind(dcid,child_nr);

	USRDEBUG("CHILD child_nr %d: remote unbinding %d\n", child_nr,client_nr);
	dvk_unbind(dcid, client_nr);

	USRDEBUG("CHILD child_nr %d:: exiting\n", child_nr);
	exit(0);
}

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
	int  svr_pid, ret, i, svr_ep, pid, child_nr, child_ep;
	double t_start, t_stop, t_total, loopbysec, tput; 
	pid_t child_pid[MAXCHILDREN];
	struct dc_usr dc, *dc_usr_ptr;
	char rmt_name[16];

  	if (argc != 4) {
    	fprintf(stderr,"usage: %s <children> <loops>  <maxbuf> \n", argv[0]);
    	exit(1);
  	}

	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	/*---------------- Get DC info ---------------*/
	dcid 	= DCID;
	dc_usr_ptr = &dc;
	ret = dvk_getdcinfo(dcid, dc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"dvk_getdcinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_usr_ptr));
	USRDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_usr_ptr));
	
	/*---------------- Check Arguments ---------------*/
	children = atoi(argv[1]);
  	loops = atoi(argv[2]);
	if( loops <= 0) {
   		fprintf(stderr, "loops must be > 0\n");
   		exit(1);
  	}
	if( children < 0 || children > (dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks) ){
   		fprintf(stderr, "children must be > 0 and < %d\n", 
			(dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks));
   		exit(1);
  	}
	
	maxbuf = atoi(argv[3]);
	if( maxbuf <= 0 || maxbuf > MAXCOPYLEN) {
   		fprintf(stderr, " 0 < maxbuf < %d\n", MAXCOPYLEN);
   		exit(1);
	}
		
	/*---------------- MAIN SERVER binding ---------------*/
	svr_pid = getpid();
    svr_ep =	dvk_bind(dcid, SVR_NR);
	if( svr_ep < 0 ) {
		fprintf(stderr, "BIND ERROR svr_ep=%d\n",svr_ep);
	}
   	USRDEBUG("BIND MAIN SERVER dcid=%d svr_pid=%d SVR_NR=%d svr_ep=%d\n",
		dcid, svr_pid, SVR_NR, 	svr_ep);

	/*---------------  remote client binding ---------*/
	sprintf(rmt_name,"client%d",CLT_NR);
	ret = dvk_rmtbind(dcid, rmt_name, CLT_NR, RMTNODE);
	if(ret < 0) {
    	fprintf(stderr,"ERROR MAIN SERVER dvk_rmtbind %d: process %s on node %d ret=%d\n", 
			CLT_NR, rmt_name, RMTNODE, ret);
    	exit(1);		
	}
   	USRDEBUG("MAIN SERVER dvk_rmtbind %d: process %s on node %d \n", 
			CLT_NR, rmt_name, RMTNODE);
		
	/*---------------- children CREATION ---------------*/
	for( i = 0; i < children; i++){	
		USRDEBUG("child fork %d\n",i);
		if( (child_pid[i] = fork()) == 0 ){/* Child */
			ret = child_function( i );
		}
		/* parent */
		USRDEBUG("MAIN SERVER child_pid[%d]=%d\n",i, child_pid[i]);
	}
		
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		fprintf(stderr, "posix_memalign\n");
   		exit(1);
	}
	USRDEBUG("MAIN SERVER m_ptr=%p\n",m_ptr);
	
	/*--------- Waiting for children START synchronization: REQUEST ---------*/
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
		do {
			ret = dvk_receive(child_nr, (long) m_ptr);
			if (ret == EDVSSRCDIED)sleep(1);
		} while (ret == EDVSSRCDIED);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_receive ret=%d\n", ret);
			exit(1);		
		}		
	}	

	/*--- Waiting START message from remote MAIN CLIENT----*/
 	USRDEBUG("MAIN SERVER: Waiting START message from remote CLIENT\n");
   	ret = dvk_receive(CLT_NR, (long) m_ptr);

	/*--------- Sending children START synchronization REPLY ---------*/
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
   		ret = dvk_send(child_nr,(long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_send ret=%d\n", ret);
			exit(1);		
		}			
	}	
	ret = dvk_send(CLT_NR, (long) m_ptr);
	t_start = dwalltime();
	
	/*--- Waiting STOP message from remote CLIENT----*/
 	USRDEBUG("MAIN SERVER: Waiting STOP message from remote CLIENT\n");
   	ret = dvk_receive(CLT_NR, (long) m_ptr);
	t_stop  = dwalltime();
	/* reply to remote MAIN CLIENT */
	ret = dvk_send(CLT_NR, (long) m_ptr);
	
	/*--------- Report statistics  ---------*/
	t_total = (t_stop-t_start);
	loopbysec = (double)(loops)/t_total;
	tput = loopbysec * (double)maxbuf;
 	printf("t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
 	printf("transfer size=%d #transfers=%d loopbysec=%f\n", maxbuf , loops, loopbysec);
 	printf("Throuhput = %f [bytes/s]\n", tput);
	
	/*--------- Waiting for children STOP synchronization: REQUEST ---------*/
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
		ret = dvk_receive(child_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_receive ret=%d\n", ret);
			exit(1);		
		}		
	}	

	/*--------- Sending children STOP synchronization  REPLY ---------*/
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
   		ret = dvk_send(child_nr,(long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_send ret=%d\n", ret);
			exit(1);		
		}			
	}	
	
	/*--------- Unbinding remote MAIN client ----*/
	dvk_unbind(dcid,CLT_NR);

	/*--------- Waiting for children EXIT ---------*/
 	USRDEBUG("MAIN SERVER: Waiting for children exit\n");
	for( i = 0; i < children; i++){	
		wait(&ret);
	}
		
	USRDEBUG("MAIN SERVER END\n");
	exit(0);
}



