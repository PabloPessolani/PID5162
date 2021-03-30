#include "dvs_run.h"

#define	 USE_WAIT4BIND	0

#define  MIGR_TEST 	16077022
#define  MAXRETRIES	10

int loops, maxbuf;
message *m_ptr;
char *buffer;
int svr_ep, dcid;
dvs_usr_t dvs, *dvs_ptr;
proc_usr_t proc_usr, *proc_ptr;
int old_nodeid;

double dwalltime()
{
	double sec;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	sec = tv.tv_sec + tv.tv_usec/1000000.0;
	return sec;
}
  
void print_usage(char *argv0){
fprintf(stderr,"Usage: %s <dcid> <svr_ep> \n", argv0 );
	fprintf(stderr,"<dcid>: DC ID (%d-%d)\n", 0, NR_DCS);
	fprintf(stderr,"<svr_ep>: server endpoint (%d-%d)\n", (-NR_TASKS), (NR_PROCS-NR_TASKS));
	ERROR_EXIT(EDVSINVAL);	
}
   
int migr_restart(void) 
{
	int rcode;
	int new_nodeid;
	
	rcode = close(dvk_fd);
	if (rcode < 0)  ERROR_PRINT(errno);

	// open the DVK pseudo-device 
	rcode = dvk_open();
	USRDEBUG("dvk_open rcode=%d\n", rcode);
	if (rcode < 0)  ERROR_EXIT(rcode);

	// Get the new node ID 
	new_nodeid = get_dvs_params();
	if( new_nodeid < 0) ERROR_EXIT(new_nodeid);
	USRDEBUG("new_nodeid=%d local_nodeid=%d\n", new_nodeid, local_nodeid);		
	if( new_nodeid != local_nodeid) { // Migrated to other node 
		old_nodeid = local_nodeid;
		// try to BIND the process 
		USRDEBUG("dcid=%d svr_ep=%d\n", dcid, svr_ep);
		rcode = dvk_bind(dcid, svr_ep);
		USRDEBUG("rcode=%d\n", rcode);
		if( rcode == EDVSSLOTUSED) {
			proc_ptr = &proc_usr;
			rcode = dvk_getprocinfo(dcid, _ENDPOINT_P(svr_ep), proc_ptr);
			USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			if( TEST_BIT(proc_ptr->p_rts_flags, BIT_REMOTE) 
			 &&!TEST_BIT(proc_ptr->p_rts_flags, MIS_BIT_RMTBACKUP)){
				
//				if( !TEST_BIT(proc_ptr->p_rts_flags, BIT_MIGRATE)) {
					rcode = dvk_migr_start(dcid, svr_ep);
					if( rcode < 0) ERROR_EXIT(rcode);
					rcode = dvk_getprocinfo(dcid, _ENDPOINT_P(svr_ep), proc_ptr);
					USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
//				}
				local_nodeid = new_nodeid;
				rcode = dvk_migr_commit(PROC_NO_PID, dcid, svr_ep, local_nodeid);					
				if( rcode < 0) ERROR_EXIT(rcode);
				rcode = dvk_getprocinfo(dcid, _ENDPOINT_P(svr_ep), proc_ptr);
				USRDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			 }
		} else {
			if( rcode != svr_ep) ERROR_EXIT(rcode);
		}
	}else {
		USRDEBUG("dcid=%d svr_ep=%d\n", dcid, svr_ep);
		rcode = dvk_bind(dcid, svr_ep);
		USRDEBUG("rcode=%d\n", rcode);
		if( rcode != svr_ep) ERROR_EXIT(rcode);
	}
	return(svr_ep);
}
 
/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
int get_dvs_params(void)
{
	int new_nodeid; 
	
	USRDEBUG("\n");
	new_nodeid = dvk_getdvsinfo(&dvs);
	USRDEBUG("new_nodeid=%d\n",new_nodeid);
	if( new_nodeid < DVS_NO_INIT) 
		ERROR_RETURN(new_nodeid);
	dvs_ptr = &dvs;
	USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	return(new_nodeid);
}
 
int  main ( int argc, char *argv[] )
{
	int  svr_pid, ret, i, retry, rcode, endpoint;
	double t_start, t_stop, t_total, loopbysec, tput; 
	proc_usr_t *proc_ptr;
	
	/*---------------- SERVER binding ---------------*/
		
	ret = dvk_open();
	USRDEBUG("dvk_open rcode=%d\n", rcode);
	if (ret < 0)  ERROR_EXIT(ret);

	local_nodeid = get_dvs_params();
	if (local_nodeid < 0) ERROR_EXIT(local_nodeid);
	
#if USE_WAIT4BIND	
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
#else //  USE_WAIT4BIND
    if( argc != 3) {
		print_usage(argv[0]);
	}
	dcid 	= atoi(argv[1]);
	svr_ep= atoi(argv[2]);

	rcode = migr_restart(); 

#endif //  USE_WAIT4BIND	
	svr_ep = rcode;
   	USRDEBUG("SERVER svr_pid=%d svr_ep=%d\n", svr_pid, svr_ep);

	/*---------------- Allocate memory for process descriptor  ---------------*/
	posix_memalign( (void **) &proc_ptr, getpagesize(), sizeof(proc_usr_t) );
	if (proc_ptr== NULL) {
   		fprintf(stderr, "SERVER: proc_usr_t posix_memalign errno=%d\n", errno);
   		exit(1);
	}
	
	ret = dvk_getprocinfo(PROC_NO_PID, _ENDPOINT_P(svr_ep), proc_ptr);
	if( ret) ERROR_EXIT(ret);
	USRDEBUG("SERVER: " PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr)); 
	
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
		////////////////////////// RECEIVING ////////////////////////////////////////
		USRDEBUG("SERVER: Receiving loops=%d\n", loops);
		do {
			ret = dvk_receive_T(ANY, (long) m_ptr, TIMEOUT_MOLCALL);
			if( ret == EDVSNOTBIND) {
				rcode =  migr_restart();
				continue;
			}
			if( ret < 0) break;
		}while( ret == EDVSNOTBIND);
		if( ret == EDVSTIMEDOUT 
			|| ret == EDVSAGAIN
			|| ret == EDVSINTR) continue;
		if( ret < 0) ERROR_EXIT(ret);
		USRDEBUG("SERVER RECEIVE:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		
		if( m_ptr->m_type != MIGR_TEST){
		   	fprintf(stderr, "SERVER: Message type not MIGR_TEST (%d)\n", m_ptr->m_type);	
		}
		
		////////////////////////// COPYING //////////////////////////////////
		do {
			ret = dvk_vcopy(svr_ep, buffer, m_ptr->m_source, m_ptr->m1_p1, m_ptr->m1_i1);
			if( ret == EDVSNOTBIND) {
				rcode =  migr_restart();
				continue;
			}
			if( ret < 0) break;
		}while( ret == EDVSNOTBIND);
		if( ret == EDVSTIMEDOUT 
			|| ret == EDVSAGAIN
			|| ret == EDVSINTR) continue;
		if( ret < 0) ERROR_EXIT(ret);

		////////////////////////// SENDING  /////////////////////////////////
		m_ptr->m_type = ret;	
		m_ptr->m1_i3  = loops;
		USRDEBUG("SERVER SEND:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		do	{
			ret = dvk_send_T(m_ptr->m_source, (long) m_ptr, TIMEOUT_MOLCALL);
			if( ret == EDVSNOTBIND) {
				rcode =  migr_restart();
				continue;
			}
			if( ret < 0) break;
		}while( ret == EDVSNOTBIND);
		if( ret == EDVSTIMEDOUT 
			|| ret == EDVSAGAIN
			|| ret == EDVSINTR) continue;
		if( ret < 0) ERROR_EXIT(ret);
			
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



