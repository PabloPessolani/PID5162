#include "loops.h"

#define 	RMTNODE	1
#define 	DCID	0
#define 	SVR_NR	0
#define 	CLT_NR	(SVR_NR + 1)
#define 	MAXCHILDREN	100

int dcid, loops, children;
message *m_ptr;

int child_function(int child) {
	int i , ret;
	pid_t child_pid;
	int child_nr,client_nr, child_ep;
	char rmt_name[16];
	
  	child_nr = SVR_NR + ((child+1) * 2);
	USRDEBUG("child %d: loops=%d child_nr=%d\n", child, loops, child_nr);
		
	/* binding local server */
	child_ep = dvk_bind(dcid, child_nr);
	child_pid = getpid();
	USRDEBUG("CHILD child=%d child_nr=%d child_ep=%d child_pid=%d\n", 
	child, child_nr, child_ep, child_pid);
	
	/* binding remote client */
	client_nr = child_nr + 1;
	sprintf(rmt_name,"client%d",client_nr);
	ret = dvk_rmtbind(dcid, rmt_name, client_nr, RMTNODE);
	if(ret < 0) {
		fprintf(stderr,"dvk_rmtbind %d: process %s on node %d \n", 
			client_nr , rmt_name, RMTNODE);
		ERROR_EXIT(ret);		
	}
	
	/* START synchronization with MAIN SERVER */
	do {
		ret = dvk_sendrec(SVR_NR, (long) m_ptr);
		if (ret == EDVSDEADSRCDST)sleep(1);
	} while (ret == EDVSDEADSRCDST);
	if(ret < 0) {
		fprintf(stderr,"CHILD %d: dvk_sendrec ret=%d\n", child, ret);
		ERROR_EXIT(ret);		
	}
		
	/* M3-IPC TRANSFER LOOP  */
 	USRDEBUG("CHILD %d: Starting loop\n", child);
	for( i = 0; i < loops; i++) {
    	ret = dvk_receive(client_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"CHILD %d: dvk_receive ret=%d\n", child, ret);
			ERROR_EXIT(ret);		
		}		
		m_ptr->m1_i1= i;
   		ret = dvk_send(client_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"CHILD %d: dvk_send ret=%d\n", child, ret);
			ERROR_EXIT(ret);		
		}	
	}
	USRDEBUG("CHILD %d:" MSG1_FORMAT, child, MSG1_FIELDS(m_ptr));

	/* STOP synchronization with MAIN SERVER */
	do {
		ret = dvk_sendrec(SVR_NR, (long) m_ptr);
		if (ret == EDVSDEADSRCDST)sleep(1);
	} while (ret == EDVSDEADSRCDST);
	if(ret < 0) {
		fprintf(stderr,"CHILD %d: dvk_sendrec ret=%d\n", child, ret);
		ERROR_EXIT(ret);		
	}
		
	USRDEBUG("CHILD %d: dvk_unbind %d\n", child, client_nr);
	dvk_unbind(dcid,client_nr);

	USRDEBUG("CHILD %d: exiting\n", child);
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
	int  svr_pid, ret, i, svr_ep, pid, child_nr;
	double t_start, t_stop, t_total, child_ep;
	pid_t child_pid[MAXCHILDREN];
	struct dc_usr dc, *dc_usr_ptr;
	char rmt_name[16];

  	if (argc < 3 || argc > 4) {
    	fprintf(stderr,"usage: %s <children> <loops> [<dcid>] \n", argv[0]);
    	exit(1);
  	}

	/*---------------- Get DC info ---------------*/
	if( argc == 3)
		dcid = DCID;
	else
		dcid 	= atoi(argv[3]);
		
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	dc_usr_ptr = &dc;
	ret = dvk_getdcinfo(dcid, dc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"dvk_getdcinfo error=%d \n", ret );
		ERROR_EXIT(ret);		
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
	
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		fprintf(stderr, "posix_memalign\n");
   		exit(1);
	}
	USRDEBUG("MAIN SERVER m_ptr=%p\n",m_ptr);	
	 
	/*---------------- MAIN SERVER binding ---------------*/
	svr_pid = getpid();
    svr_ep =  dvk_bind(dcid, SVR_NR);
	if( svr_ep < 0 ) {
		fprintf(stderr, "BIND ERROR svr_ep=%d\n",svr_ep);
	}
   	USRDEBUG("BIND MAIN SERVER dcid=%d svr_pid=%d SVR_NR=%d svr_ep=%d\n",
		dcid, svr_pid, SVR_NR, 	svr_ep);
	
	/*--------------- remote client binding ---------*/
	sprintf(rmt_name,"client%d",CLT_NR);
	ret = dvk_rmtbind(dcid, rmt_name, CLT_NR, RMTNODE);
	if(ret < 0) {
    	fprintf(stderr,"ERROR MAIN SERVER dvk_rmtbind %d: process %s on node %d ret=%d\n", 
			CLT_NR, rmt_name, RMTNODE, ret);
		ERROR_EXIT(ret);		
	}
   	USRDEBUG("MAIN SERVER dvk_rmtbind %d: process %s on node %d \n", 
			CLT_NR, rmt_name, RMTNODE);
	
	/*---------------- children creation ---------------*/
	for( i = 0; i < children; i++){	
		USRDEBUG("child fork %d\n",i);
		if( (child_pid[i] = fork()) == 0 ){/* Child */
			ret = child_function( i );
		}
		/* parent */
		USRDEBUG("MAIN SERVER child_pid[%d]=%d\n",i, child_pid[i]);
	}
								
	/*--------- Waiting for START synchronization from children ---------*/
	USRDEBUG("MAIN SERVER: START synchronization from %d children: REQUEST\n", children);
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
		do {
			ret = dvk_receive(child_nr, (long) m_ptr);
			if (ret == EDVSSRCDIED)sleep(1);
		} while (ret == EDVSSRCDIED);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_receive ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}
	}
	/*--------------- Reply to  children -------------------------------*/
	USRDEBUG("MAIN SERVER: START synchronization from %d children: REPLY \n", children);
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
   		ret = dvk_send(child_nr,(long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_send ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}			
	}	
	
	/*--- Waiting START message from remote CLIENT----*/
 	USRDEBUG("MAIN SERVER: Waiting START message from remote CLIENT\n");
   	ret = dvk_receive(ANY, (long) m_ptr);
	if( m_ptr->m_source != CLT_NR){
		fprintf(stderr,"ERROR MAIN SERVER: m_source(%d) != %d\n", m_ptr->m_source, CLT_NR);
		ERROR_EXIT(ret);		
	}
	ret = dvk_send(CLT_NR, (long) m_ptr);
	t_start = dwalltime();

	/*--- Waiting STOP message from remote MAIN CLIENT----*/
 	USRDEBUG("MAIN SERVER: Waiting STOP message from remote MAIN CLIENT\n");
   	ret = dvk_receive(CLT_NR, (long) m_ptr);
	t_stop  = dwalltime();
	/* reply to remote MAIN CLIENT */
	ret = dvk_send(CLT_NR, (long) m_ptr);
	
	/*--------- Report statistics  ---------*/
	t_total = (t_stop-t_start);
 	printf("t_start=%.2f t_stop=%.2f t_total=%.2f\n",t_start, t_stop, t_total);
 	printf("Children = %d\n", children);
 	printf("Loops = %d\n", loops);
 	printf("Total msg transfers = %d\n", loops*children);
 	printf("Time for a pair of SENDREC/RECEIVE-SEND= %f[ms]\n", 1000*t_total/(double)(loops*children));
 	printf("Throuhput = %f [SENDREC/RECEIVE-SEND/s]\n", (double)(loops*children)/t_total);
	
	/*--------- STOP synchronization from children: REQUEST ---------*/
	USRDEBUG("MAIN SERVER: STOP synchronization from %d children: REQUEST\n", children);
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
		do {
			ret = dvk_receive(child_nr, (long) m_ptr);
			if (ret == EDVSSRCDIED)sleep(1);
		} while (ret == EDVSSRCDIED);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_receive ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}
	}
	/*--------- STOP synchronization from children: REPLY ---------*/
	USRDEBUG("MAIN SERVER: STOP synchronization from %d children: REPLY\n", children);
	for( i = 0; i < children; i++){
	  	child_nr = SVR_NR + ((i+1) * 2);
   		ret = dvk_send(child_nr,(long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN SERVER: dvk_send ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}			
	}
	
	
	/*--------- Waiting for children EXIT ---------*/
 	USRDEBUG("MAIN SERVER: Waiting for children exit\n");
	for( i = 0; i < children; i++){	
		wait(&ret);
	}

	/*--------- Unbinding remote MAIN client ----*/
	USRDEBUG("MAIN SERVER: dvk_unbind %d\n", CLT_NR);
	dvk_unbind(dcid,CLT_NR);
	USRDEBUG("MAIN SERVER END\n");

}



