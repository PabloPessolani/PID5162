#include "tests.h"
   
void  main ( int argc, char *argv[] )
{
	int dcid, pid, p_nr, ret, ep; 
	int nodeid;

	if ( argc != 3) {
		fprintf(stderr,  "Usage: %s <dcid> <p_nr> \n", argv[0] );
		exit(EXIT_FAILURE);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}

	p_nr = atoi(argv[2]);
	if ( p_nr < (-NR_TASKS) || p_nr >= (NR_PROCS)) {
		fprintf(stderr,  "Invalid p_nr [(%d)-(%d)]\n", -NR_TASKS, NR_PROCS-1);
		exit(EXIT_FAILURE);
	}
	

	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);

	pid = getpid();
    printf(" dvk_ukbind (%d): p_nr=%d DC%d \n", pid, p_nr,dcid);
	ep = dvk_ukbind(dcid,pid,p_nr);
	if( ep < EDVSERRCODE) ERROR_EXIT(ep);
	sleep(60);
	exit(0);
 }



