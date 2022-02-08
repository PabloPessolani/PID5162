#include "tests.h"
   
   
void  main ( int argc, char *argv[] )
{
	int dcid, self_ep, src_ep, ret;
	message *m_ptr;

	m_ptr = (char *) malloc(sizeof(message));
	memset(m_ptr,0x00,sizeof(message));
	
    if ( argc != 4) {
		fprintf(stderr,  "Usage: %s <dcid> <self_ep> <src_ep>  \n", argv[0] );
		exit(EXIT_FAILURE);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}

	self_ep = atoi(argv[2]);
	src_ep = atoi(argv[3]);
	
	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);
	
	self_ep =	dvk_bind(dcid, self_ep);
	if( self_ep < EDVSERRCODE) ERROR_EXIT(self_ep);
	
	printf("SELF BIND dcid=%d self_ep=%d src_ep=%d\n",
				dcid, self_ep, src_ep);
			

	ret = dvk_receive(src_ep, (long) m_ptr);
	if( ret < 0 ) ERROR_PRINT(ret);
	printf(MSG1_FORMAT, MSG1_FIELDS(m_ptr));
	
	sleep(60);
}




