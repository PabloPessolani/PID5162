#include "tests.h"
   
void  main ( int argc, char *argv[] )
{
	int dcid, pid, p_nr, ret, ep; 
	int nodeid;

	if ( argc != 5) {
		fprintf(stderr,  "Usage: %s <dcid> <p_nr> <nodeid> <name> \n", argv[0] );
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
	
	nodeid = atoi(argv[3]);
	if ( nodeid < 0 || nodeid >= NR_NODES) {
		fprintf(stderr,  "Invalid nodeid [0-%d]\n", NR_NODES-1 );
		exit(EXIT_FAILURE);
	}
	
	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);

    printf(" dvk_rmtbind %s with p_nr=%d to DC%d on node=%d\n",argv[4],p_nr,dcid,nodeid);
	ep = dvk_rmtbind(dcid,argv[4],p_nr,nodeid);
	if( ep < EDVSERRCODE) ERROR_EXIT(ep);
	
 }



