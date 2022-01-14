#include "dvs_run.h"
   
void  main ( int argc, char *argv[] )
{
	int dcid, parent_pid, child_pid, parent_nr, child_nr; 
	int status, rcode, parent_ep, child_ep, ret, i;
	
    if ( argc != 4) {
		fprintf(stderr,  "Usage: %s <dcid> <parent_nr> <child_nr> \n", argv[0] );
 	    exit(EXIT_FAILURE);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}

	parent_nr = atoi(argv[2]);
	parent_pid = getpid();
	child_nr = atoi(argv[3]);
	
	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);
	
    printf("PARENT: binding itself (%d) to DC%d with parent_nr=%d\n",parent_pid,dcid,parent_nr);
	parent_ep = dvk_bind(dcid, parent_nr); 
	if( parent_ep < EDVSERRCODE) ERROR_EXIT(parent_ep);

	printf("PARENT: waiting for child binding: %d\n", child_nr);
	for( i = 0; i < 30; i++){ 
		rcode = dvk_wait4bindep_T(child_nr, 1000);
		if(rcode < 0 ) ERROR_EXIT(rcode);
	}
	exit(0);
 }



