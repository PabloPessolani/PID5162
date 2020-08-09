#define  MOL_USERSPACE	1

#include <asm/ptrace.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
//#include "stub_syscall.h"
#include "./kernel/minix/config.h"
#include "../tasks/memory/memory.h"
//#include "./kernel/minix/kipc.h"
//#include "./kernel/minix/callnr.h"
//#include "limits.h"

//#define BUFFER 20
//#define nro_IOREQS 3 /*por ahora sólo pruebo con un requerimiento, para la longitud del vector*/
//#define WRITE 2000

void  main ( int argc, char *argv[] )
{
    int vmid;
    int clt_pid, svr_pid;
    int clt_ep, svr_ep;
    int clt_nr, svr_nr, ret, proc_nr;
	off_t position;
	unsigned count;
    message m;
	int i;

    int bytesCount = 0;

	
	/*    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS*/
	/* ----------------------------------------------------------------*/
	/* |  DEV_OPEN  | device  | proc nr |         |         |         |*/
	//	m.m_type		m.m2_i1	m.m2_i2		

		

	if ( argc != 5)
    {
        printf( "Usage: %s <vmid> <clt_nr> <svr_nr> <minor_dev>\n", argv[0] );
        exit(1);
    }

	/*verifico q el nro de vm sea válido*/
    vmid = atoi(argv[1]);
    if ( vmid < 0 || vmid >= NR_VMS)
    {
        printf( "Invalid vmid [0-%d]\n", NR_VMS - 1 );
        exit(1);
    }

	/*entiendo nro cliente "cualquiera" - sería este proceso el q va a ser el cliente*/
    clt_nr = atoi(argv[2]);
	
	/*server endpoint es el q ya tengo de memory*/
    svr_ep = svr_nr = atoi(argv[3]);

	/*hago el binding del proceso cliente, o sea este*/
    clt_pid = getpid();
    clt_ep = mnx_bind(vmid, clt_nr);
	proc_nr = clt_ep;
	
    if ( clt_ep < 0 )
        printf("BIND ERROR clt_ep=%d\n", clt_ep);

    printf("BIND CLIENT vmid=%d clt_pid=%d clt_nr=%d clt_ep=%d\n",
           vmid,
           clt_pid,
           clt_nr,
           clt_ep);
		   
	printf("CLIENT pause before SENDREC\n");
	sleep(2); 

    m.m_type = DEV_CLOSE;
	printf("m_type: %d -  DEV_CLOSE: %d\n", m.m_type, DEV_CLOSE);
	
    m.DEVICE = atoi(argv[4]); 
	printf("DEVICE: %d\n", m.DEVICE);

    m.IO_ENDPT = proc_nr; //process doing the request (usado en memory.c)
	printf("DEV_CLOSE - proc_nr: m_m2_i2=%d\n", m.IO_ENDPT);	

	printf("SENDREC msg  m_type=%d, DEVICE=%d, IO_ENDPT=%d\n",
           m.m_type,
           m.DEVICE,
		   m.IO_ENDPT);

    ret = mnx_sendrec(svr_ep, (long) &m); /*para enviar y recibir de un cliente a un servidor*/
	if( ret != 0 )
    	printf("SEND ret=%d\n",ret);

	
	sleep(2);
	
	printf("REPLY: -> m_type=%d, (REP_ENDPT)=%d, (REP_STATUS)=%d\n",
		m.m_type,
		m.REP_ENDPT,
		m.REP_STATUS);	
		
	while(1){
		printf("Ctrl-C to exit\n");
		sleep(1);
	}
	
 }

		   
    //ret = mnx_sendrec(svr_ep, (long) &m);
    //if ( ret != OK )
    //    printf("ERROR SENDREC ret=%d\n", ret);
    //else
    //    printf("SENDREC ret=%d\n", ret);

    /*printf("REPLY OPEN msg: m_source=%d, m_type=%d, m1_i1=%d, m1_i2=%d, m1_p1=%s\n",
           m.m_source,
           m.m_type,
           m.m1_i1,
           m.m1_i2,
           m.m1_p1);

    fileDescriptor = m.m1_i1;
    
    /*READ!!!!*********************************************************************/
    /*printf("CLIENT pause before SENDREC (READ)\n");
    sleep(2);*/

//}