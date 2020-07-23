#include "tests.h"
   
char *buffer;
   
void  main ( int argc, char *argv[] )
{
	int parent_pid, parent_ep, parent_nr;
	int child_pid, child_ep, child_nr;
	int i, dcid,  ret;
	priv_usr_t priv_usr, *priv_ptr;
	message *m_ptr;
	
	m_ptr = (message *) malloc(sizeof(message));
	memset((char *) m_ptr,0x00,sizeof(message));
	
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
	child_ep = child_nr = atoi(argv[3]);
	
	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);
	
	parent_pid = getpid();
	priv_ptr = &priv_usr;
	
	if( child_pid = fork() ) { //  PARENT
	
		parent_ep =	dvk_bind(dcid, parent_nr);
		if( parent_ep < EDVSERRCODE) ERROR_PRINT(parent_ep);
		printf("PARENT BIND dcid=%d parent_pid=%d parent_nr=%d parent_ep=%d m_ptr=%p\n",
				dcid,
				parent_pid,
				parent_nr,
				parent_ep,
				m_ptr);
				
		// CHECK PRIVILEGES BEFORE
		ret = dvk_getpriv(dcid , parent_ep, priv_ptr);
		if( ret != 0 ) ERROR_PRINT(ret);
		printf("PARENT PRIV BEFORE :" PRIV_USR_FORMAT, PRIV_USR_FIELDS(priv_ptr));		

		printf("PARENT pause before SENDREC\n");
		// WAIT CHILD 
		sleep(3); 
		m_ptr->m_type= 0x0A;
		m_ptr->m1_i1 = 0x01;
		m_ptr->m1_i2 = 0x02;
		m_ptr->m1_i3 = 0x03;
				
		//  SENDREC MESSAGE
		printf("PARENT SENDREC msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
		ret = dvk_sendrec(child_ep, (long) m_ptr);
		if( ret != 0 ) ERROR_PRINT(ret);
		printf("PARENT REPLY msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));
				
		// CHECK PRIVILEGES
		ret = dvk_getpriv(dcid , parent_ep, priv_ptr);
		if( ret != 0 ) ERROR_PRINT(ret);
		printf("PARENT PRIV AFTER :" PRIV_USR_FORMAT, PRIV_USR_FIELDS(priv_ptr));	
		wait(0);
	}else{ // CHILD 
		
		child_pid = getpid();
		child_ep =	dvk_bind(dcid, child_nr);
		if( child_ep < EDVSERRCODE) ERROR_PRINT(child_ep);

		printf("CHILD BIND dcid=%d child_pid=%d child_nr=%d child_ep=%d m_ptr=%p\n",
				dcid, child_pid,child_nr,child_ep,m_ptr);
				
		printf("CHILD RECEIVE\n");
		ret = dvk_receive(ANY, (long) m_ptr);
		if( ret != 0 ) ERROR_PRINT(ret);
		printf("CHILD RECEIVE msg:" MSG1_FORMAT, MSG1_FIELDS(m_ptr));	

  		priv_ptr->priv_warn = 0x55;
 		priv_ptr->priv_level = KERNEL_PRIV;
		printf("CHILD SET PRIV:" PRIV_USR_FORMAT, PRIV_USR_FIELDS(priv_ptr));

		ret = dvk_setpriv(dcid , m_ptr->m_source , priv_ptr);
		if( ret != 0 ) ERROR_PRINT(ret);
		
		printf("CHILD SEND\n");
		m_ptr->m_type = 0xFE;
		ret = dvk_send(m_ptr->m_source,(long) m_ptr);
		if( ret != 0 ) ERROR_PRINT(ret);
	}
	
 }



