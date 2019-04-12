#include "loops.h"

#define 	RMTNODE	0
#define 	DCID	0
#define 	CLT_NR	1
#define 	SVR_NR	(CLT_NR - 1)
#define 	MAXCHILDREN	100

int dcid, loops, children;
message *m_ptr;

int child_function(int child) {
	int i , ret;
	pid_t child_pid;
	int child_nr, server_nr, child_ep;
	char rmt_name[16];
	
	child_nr = CLT_NR + ((child+1) * 2);
	printf("child %d: loops=%d child_nr=%d\n", child, loops, child_nr);

	/* binding remote server */
	server_nr = child_nr-1;
	sprintf(rmt_name,"server%d",server_nr);
	ret = dvk_rmtbind(dcid, rmt_name, server_nr, RMTNODE);
	if(ret < 0) {
		fprintf(stderr,"dvk_rmtbind %d: process %s on node %d \n", 
			server_nr , rmt_name, RMTNODE);
		ERROR_EXIT(ret);		
	}
	
	/* binding local client */
  	child_nr = CLT_NR + ((child+1) * 2);
	child_ep = dvk_bind(dcid, child_nr);
	child_pid = getpid();
	printf("CHILD child=%d child_nr=%d child_ep=%d child_pid=%d\n", 
	child, child_nr, child_ep, child_pid);

	/* START synchronization with MAIN CLIENT */
	do {
		ret = dvk_sendrec(CLT_NR, (long) m_ptr);
		if (ret == EDVSDEADSRCDST)sleep(1);
	} while (ret == EDVSDEADSRCDST);
	if(ret < 0) {
		fprintf(stderr,"CHILD %d: dvk_sendrec ret=%d\n", child, ret);
		ERROR_EXIT(ret);		
	}	
	
	/* M3-IPC TRANSFER LOOP  */
 	printf("CHILD %d: Starting loop\n", child);
	for( i = 0; i < loops; i++) {
		m_ptr->m1_i2= i;
   		ret = dvk_sendrec(server_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"CHILD %d: dvk_sendrec ret=%d\n", child, ret);
		ERROR_EXIT(ret);		
		}		
	}
	printf("CHILD %d:" MSG1_FORMAT, child, MSG1_FIELDS(m_ptr));

	/* STOP synchronization with MAIN CLIENT */
	ret = dvk_sendrec(CLT_NR, (long) m_ptr);
	if(ret < 0) {
		fprintf(stderr,"CHILD %d: dvk_sendrec ret=%d\n", child, ret);
		ERROR_EXIT(ret);		
	}	
	
	dvk_unbind(dcid,server_nr);
	printf("CHILD %d: exiting\n", child);
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
	int  clt_pid, ret, i, pid, clt_ep,child_ep, child_nr;
	double t_start, t_stop, t_total;
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
	printf("MAIN CLIENT m_ptr=%p\n",m_ptr);	
	
	/*---------------- MAIN CLIENT binding --------------*/
	clt_pid = getpid();
    clt_ep =	dvk_bind(dcid, CLT_NR);
	if( clt_ep < 0 ) {
		fprintf(stderr, "BIND ERROR clt_ep=%d\n",clt_ep);
	}
   	printf("BIND MAIN CLIENT dcid=%d clt_pid=%d CLT_NR=%d clt_ep=%d\n",
		dcid, clt_pid, CLT_NR, 	clt_ep);
		
		
	/*---------------- children  creation ---------------*/
	for( i = 0; i < children; i++){	
		printf("child fork %d\n",i);
		if( (child_pid[i] = fork()) == 0 ){/* Child */
			ret = child_function( i );
		}
		/* parent */
		printf("MAIN CLIENT child_pid[%d]=%d\n",i, child_pid[i]);
	}
	
	/*--------- Waiting for children START synchronization: REQUEST ---------*/
	printf("MAIN CLIENT: START %d children synchronization: REQUEST\n",children  );
	for( i = 0; i < children; i++){
		child_nr = CLT_NR + ((i+1) * 2);
		do {
			ret = dvk_receive(child_nr, (long) m_ptr);
			if (ret == EDVSSRCDIED)sleep(1);
		} while (ret == EDVSSRCDIED);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN CLIENT: dvk_receive ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}			
	}		
	
	/*--------------- binding remote server ---------*/
	sprintf(rmt_name,"server%d",SVR_NR);
	ret = dvk_rmtbind(dcid, rmt_name, SVR_NR, RMTNODE);
	if(ret < 0) {
    	fprintf(stderr,"ERROR MAIN CLIENT dvk_rmtbind %d: process %s on node %d \n", 
			SVR_NR, rmt_name, RMTNODE);
		ERROR_EXIT(ret);		
	}
   	printf("MAIN CLIENT dvk_rmtbind %d: process %s on node %d \n", 
			SVR_NR, rmt_name, RMTNODE);
			
	/*--- Sending START message to remote SERVER ----*/
 	printf("MAIN CLIENT: Sending START message to remote SERVER\n");
	ret = dvk_sendrec(SVR_NR, (long) m_ptr);

	/*--------- Sending for children  START synchronization : REPLY ---------*/
	printf("MAIN CLIENT: START %d children synchronization: REPLY \n",children  );
	for( i = 0; i < children; i++){
		child_nr = CLT_NR + ((i+1) * 2);
   		ret = dvk_send(child_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN CLIENT: dvk_send ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}			
	}	
	
	/*--------- Waiting for children  STOP synchronization ---------*/
	printf("MAIN CLIENT: STOP %d children synchronization: REQUEST\n",children  );
	for( i = 0; i < children; i++){
		child_nr = CLT_NR + ((i+1) * 2);
    	ret = dvk_receive(child_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN CLIENT: dvk_receive ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}		
	}	

	/*--- Sending STOP message to remote SERVER ----*/
 	printf("MAIN CLIENT: Sending STOP message to remote SERVER\n");
	ret = dvk_sendrec(SVR_NR, (long) m_ptr);

	/*--------- Sending replies to children --------*/
	printf("MAIN CLIENT: STOP %d children synchronization: REPLY \n",children  );
	for( i = 0; i < children; i++){
		child_nr = CLT_NR + ((i+1) * 2);
   		ret = dvk_send(child_nr, (long) m_ptr);
		if(ret < 0) {
			fprintf(stderr,"ERROR MAIN CLIENT: dvk_send ret=%d\n", ret);
			ERROR_EXIT(ret);		
		}			
	}	
	
	sleep(3);
	dvk_unbind(dcid,SVR_NR);
	
	/*--------- Waiting for children EXIT ---------*/
	for( i = 0; i < children; i++){	
		wait(&ret);
	}
	
	printf("MAIN CLIENT END\n");

}



