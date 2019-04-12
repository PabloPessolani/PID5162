#include "loops.h"

#define		RANDOM_PATERN	0

int dcid, maxbuf;
message *m_ptr;
char *buffer;

#define MAXSTRLEN		1024 

int local_nodeid;
 
int  main ( int argc, char *argv[] )
{
	int  clt_pid, ret, i, clt_ep, svr_ep;
	dc_usr_t dc, *dc_usr_ptr;
	proc_usr_t proc, *proc_usr_ptr;
	dvs_usr_t dvs, *dvs_usr_ptr;

  	if (argc != 4) {
    	fprintf(stderr,"usage: %s <dcid> <svr_ep> <clt_ep>\n", argv[0]);
    	exit(1);
  	}

	/*-------- Open of pseudo DVK device --------------*/
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	/*---------------- Get DVS info ---------------*/
	dvs_usr_ptr = &dvs;
	ret = dvk_getdvsinfo(dvs_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"dvk_getdvsinfo error=%d \n", ret );
 	    exit(1);
	}
	local_nodeid = ret;
	USRDEBUG("local_nodeid=%d\n",local_nodeid);
	USRDEBUG(DVS_USR_FORMAT,DVS_USR_FIELDS(dvs_usr_ptr));
	
	/*---------------- Get DC info ---------------*/
	dcid 	= atoi(argv[1]);
	dc_usr_ptr = &dc;
	ret = dvk_getdcinfo(dcid, dc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"dvk_getdcinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(DC_USR1_FORMAT,DC_USR1_FIELDS(dc_usr_ptr));
	USRDEBUG(DC_USR2_FORMAT,DC_USR2_FIELDS(dc_usr_ptr));
	
	/*---------------- Check Arguments ---------------*/
	svr_ep = atoi(argv[2]);
	if( svr_ep < 0 || svr_ep > (dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks) ){
   		fprintf(stderr, "svr_ep must be > 0 and < %d\n", 
			(dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks));
   		exit(1);
  	}
	
	clt_ep = atoi(argv[3]);
	if( clt_ep < 0 || clt_ep > (dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks) ){
   		fprintf(stderr, "clt_ep must be > 0 and < %d\n", 
			(dc_usr_ptr->dc_nr_sysprocs - dc_usr_ptr->dc_nr_tasks));
   		exit(1);
  	}
		
	if( svr_ep == clt_ep)	{
   		fprintf(stderr, "clt_ep must be != svr_ep\n");
   		exit(1);
  	}
			
	/*----------------  CLIENT binding ---------------*/
	clt_pid = getpid();
    ret =  dvk_bind(dcid, clt_ep);
	if( ret < 0 ) {
		fprintf(stderr, "BIND ERROR ret=%d\n",ret);
	}
   	USRDEBUG("BIND CLIENT dcid=%d clt_pid=%d clt_ep=%d\n",
		dcid, clt_pid, clt_ep);

	/*---------------- Get CLIENT PROC info ---------------*/
	proc_usr_ptr = &proc;
	ret = dvk_getprocinfo(dcid, clt_ep, proc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"CLIENT dvk_getprocinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_usr_ptr));
	
	/*---------------- Get SERVER PROC info ---------------*/
	proc_usr_ptr = &proc;
	ret = dvk_getprocinfo(dcid, svr_ep, proc_usr_ptr);
	if(ret < 0) {
 	    fprintf(stderr,"SERVER dvk_getprocinfo error=%d \n", ret );
 	    exit(1);
	}
	USRDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_usr_ptr));
	
	/*---------------- Allocate memory for message  ---------------*/
	posix_memalign( (void **) &m_ptr, getpagesize(), sizeof(message) );
	if (m_ptr== NULL) {
   		fprintf(stderr, "posix_memalign\n");
   		exit(1);
	}
	USRDEBUG("CLIENT m_ptr=%p\n",m_ptr);
	
	/*---------------- Allocate memory for DATA BUFFER ---------------*/
	posix_memalign( (void**) &buffer, getpagesize(), MAXSTRLEN);
	if (buffer== NULL) {
   		fprintf(stderr, "CLIENT: buffer posix_memalign %d\n", errno);
   		exit(1);
  	}
	
	/*---------------- Fill with EDVSacters the DATA BUFFER ---------------*/
	srandom( getpid());
	
	for(i = 0; i < MAXSTRLEN-3; i++){
#define MAX_ALPHABET ('z' - '0')
#if RANDOM_PATERN
		buffer[i] =  (random()/(RAND_MAX/MAX_ALPHABET)) + '0';
#else	
		buffer[i] = ((i%25) + 'a');	
#endif
	}
	buffer[MAXSTRLEN-2] = '\n';
	buffer[MAXSTRLEN-1] = 0;
	USRDEBUG("CLIENT: buffer before=%s\n", buffer);
		
	m_ptr->m1_p1 = buffer;
	m_ptr->m1_i1 = MAXSTRLEN;
	USRDEBUG("CLIENT SENT: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));

	/* reply to remote client */
	ret = dvk_sendrec(svr_ep, (long) m_ptr);
	if(ret < 0) {
		fprintf(stderr,"CLIENT: dvk_send ret=%d\n", ret);
		exit(1);		
	}	
	USRDEBUG("CLIENT RECIEVED: " MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	
	USRDEBUG("CLIENT: buffer after=%s\n", buffer);

	USRDEBUG("CLIENT END\n");
	exit(0);
}



