#define _GNU_SOURCE     
#define _MULTI_THREADED
#define _TABLE
#include "radar.h"
#define TRUE 1   

int main ( int argc, char *argv[] )
{
    int dcid, clt_pid, clt_ep, clt_nr, svr_nr, ret, i;
    message *m_ptr;
    proc_usr_t usr_info, *p_usr;
    
    p_usr = &usr_info;
    m_ptr = (message *) malloc(sizeof(message));
    memset(m_ptr,0x00,sizeof(message));
    
    if ( argc != 4) {
        fprintf(stderr,  "Usage: %s <dcid> <svr_nr> <clt_nr> \n", argv[0] );
        exit(EXIT_FAILURE);
    }
    
    dcid = atoi(argv[1]);
    if ( dcid < 0 || dcid >= NR_DCS) {
        fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
        exit(EXIT_FAILURE);
    }
    
    svr_nr = atoi(argv[2]);
    clt_nr = atoi(argv[3]);
    
    ret = dvk_open();
    if (ret < 0)  ERROR_PRINT(ret);
    
    ret = dvk_getprocinfo(dcid, svr_nr, p_usr);
    if( ret < 0) ERROR_EXIT(ret);
    printf(PROC_USR_FORMAT, PROC_USR_FIELDS(p_usr));
    printf(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(p_usr));
    printf(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(p_usr));
    
    clt_pid = getpid();
    clt_ep = dvk_bind(dcid, clt_nr);
    if( clt_ep < EDVSERRCODE) ERROR_PRINT(clt_ep);
    
    printf("CLIENT BIND dcid=%d clt_pid=%d clt_nr=%d clt_ep=%d m_ptr=%p\n",
           dcid,
           clt_pid,
           clt_nr,
           clt_ep,
           m_ptr);
    
    printf("CLIENT pause before SENDREC\n");
    // WAIT CHILD 
    sleep(3); 
    m_ptr->m_type= 0x0A;
    m_ptr->m1_i1 = 0x01;
    m_ptr->m1_i2 = 0x02;
    m_ptr->m1_i3 = 0x03;
    
    // FIRST SENDREC MESSAGE
#define RADAR_TIMEOUT 		5000
#define MAX_RETRIES 		30

	for( i = 0;  i < MAX_RETRIES; i++){
	    m_ptr->m1_i1 = i;
		printf("CLIENT SENDREC msg %d:" MSG1_FORMAT, i, MSG1_FIELDS(m_ptr));
		ret = dvk_sendrec_T(svr_nr, (long) m_ptr, RADAR_TIMEOUT);
		if( ret == 0 ) 
			ERROR_PRINT(ret);
		else 
			printf("CLIENT received  REPLY msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		sleep(1);
	}
	
    exit(0);
}




