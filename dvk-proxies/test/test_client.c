#include "loops.h"


#define WAITTIME	3

   
void  main ( int argc, char *argv[] )
{
	int dcid, pid, src_nr, src_ep, dst_ep, ret, nodeid;
	message m;

   	if ( argc != 5) {
 	     printf( "Usage: %s <dcid> <src_ep> <dst_ep> <dst_nodeid>\n", argv[0] );
 	     exit(1);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
 	        printf( "Invalid dcid [0-%d]\n", NR_DCS-1 );
 	        exit(1);
	}

	src_nr = atoi(argv[2]);
	dst_ep = atoi(argv[3]);
	nodeid = atoi(argv[4]);
	pid = getpid();

	ret = dvk_open();
	if (ret < 0)  ERROR_EXIT(ret);
	
	do {
	   	src_ep = dvk_bind(dcid, src_nr);
		if( (src_ep != EDVSBUSY) && (src_ep < 0))			
			exit(src_ep);
		src_nr++;
	}while( src_ep == EDVSBUSY );

    printf("Binding process %d to DC%d with src_nr=%d src_ep=%d\n",pid,dcid,src_nr,src_ep);

	ret = dvk_rmtbind(dcid, "test_server", dst_ep, nodeid);

  	m.m_type= 0xFF;
	m.m1_i1 = src_nr;
	m.m1_i2 = 0x02;
	m.m1_i3 = 0x03;
   	printf("SEND msg: m_type=%d, m1_i1=%d, m1_i2=%d, m1_i3=%d\n",
		m.m_type,
		m.m1_i1,
		m.m1_i2,
		m.m1_i3);
    ret = dvk_send(dst_ep, (long) &m);
	if( ret != 0 )
	    	printf("SEND ret=%d\n",ret);

	printf("CLIENT sleep before UNBIND\n");
	sleep(WAITTIME);
	dvk_unbind(dcid, src_ep);
	dvk_unbind(dcid, dst_ep);

 }



