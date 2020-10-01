#define  MOL_USERSPACE	1
#define TASKDBG		1

#include <asm/ptrace.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "../../../kernel/minix/config.h"

#include "../../../tasks/rdisk/rdisk.h"
#include "../../../kernel/minix/syslib.h"
#include "../../../kernel/minix/proc_usr.h"

#define BUFFER 80000
#define nro_IOREQS 3 /*por ahora sólo pruebo con un requerimiento, para la longitud del vector*/
#define WRITE 2000

#define WAIT4BIND_MS 1000 
int vmid, clt_nr, clt_lpid;
int local_nodeid;
drvs_usr_t drvs, *drvs_ptr;
proc_usr_t rd, *rd_ptr;
proc_usr_t clt, *clt_ptr;
VM_usr_t  vmu, *vm_ptr;
int clt_ep, svr_ep;

/*===========================================================================*
 *				init_m3ipc					     *
 *===========================================================================*/
void init_m3ipc(void)
{
	int rcode;

	clt_lpid = getpid();
	do {
		rcode = mnx_wait4bind_T(WAIT4BIND_MS);
		TASKDEBUG("CLIENT mnx_wait4bind_T  rcode=%d\n", rcode);
		if (rcode == EMOLTIMEDOUT) {
			TASKDEBUG("CLIENT mnx_wait4bind_T TIMEOUT\n");
			continue ;
		} else if ( rcode < 0)
			ERROR_EXIT(EXIT_FAILURE);
	} while	(rcode < OK);

	TASKDEBUG("Get the DRVS info from SYSTASK\n");
	rcode = sys_getkinfo(&drvs);
	if (rcode) ERROR_EXIT(rcode);
	drvs_ptr = &drvs;
	TASKDEBUG(DRVS_USR_FORMAT, DRVS_USR_FIELDS(drvs_ptr));

	TASKDEBUG("Get the VM info from SYSTASK\n");
	rcode = sys_getmachine(&vmu);
	if (rcode) ERROR_EXIT(rcode);
	vm_ptr = &vmu;
	TASKDEBUG(VM_USR_FORMAT, VM_USR_FIELDS(vm_ptr));

	TASKDEBUG("Get RDISK info from SYSTASK\n");
	rcode = sys_getproc(&rd, RDISK_PROC_NR);
	if (rcode) ERROR_EXIT(rcode);
	rd_ptr = &rd;
	TASKDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(rd_ptr));
	if ( TEST_BIT(rd_ptr->p_rts_flags, BIT_SLOT_FREE)) {
		fprintf(stderr, "RDISK not started\n");
		fflush(stderr);
		ERROR_EXIT(EMOLNOTBIND);
	}

	TASKDEBUG("Get Client info from SYSTASK\n");
	rcode = sys_getproc(&clt, SELF);
	if (rcode) ERROR_EXIT(rcode);
	clt_ptr = &clt;
	TASKDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(clt_ptr));
	clt_ep = clt_ptr->p_endpoint;
	TASKDEBUG("Endpoint 02_test_devwrite: %d\n", clt_ep);

}

void  main ( int argc, char *argv[] )
{
    int ret, proc_nr;
	off_t position;
	unsigned count;
    message m;
	int i;

    int bytesCount = 0;

	char buffer[BUFFER]; //,buffer1[BUFFER],buffer2[BUFFER];

	for ( i = 0 ; i < (BUFFER); i++) 
		buffer[i] = 'Z';
		
	TASKDEBUG("En el cliente buffer: %s\n", buffer);
	
	if ( argc != 4){
        printf( "Usage: %s <position> <bytesCount> <minor_number> \n", argv[0] );
        exit(1);
    }

	
	/*BIND - DEMONIZE*/
	
	init_m3ipc();
	
	/*BIND - DEMONIZE*/

	TASKDEBUG("CLIENT pause before SENDREC\n");
	sleep(2); 
	

	svr_ep = RDISK_PROC_NR;
		
	proc_nr = clt_ep;
	
	position = atoi(argv[1]);
    
    bytesCount = atoi(argv[2]);
    if ( bytesCount == 0)
    {
        printf( "bytesCount no debe ser CERO:%d\n", bytesCount);
        exit(1);
    }
	
    // armado del mensaje, siguiendo lo indicado en driver.c
	//MENSAJE TIPO m2
	//*    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
	 //  DEV_WRITE		device	 proc nr	bytes	  offset	buf ptr
	
	//	m.m_type		m.m2_i1	m.m2_i2		m.m2_l2	 m.m2_l1	m.m2_p1
	
    m.m_type = DEV_WRITE;
	TASKDEBUG("DEV_WRITE: m_type=%d\n", m.m_type);
	
	m.DEVICE = atoi(argv[3]);
	TASKDEBUG("DEV_WRITE - Device: m_m2_i1=%d\n", m.DEVICE);
	
    m.IO_ENDPT = proc_nr; //process doing the request (usado en memory.c)
	TASKDEBUG("DEV_WRITE - proc_nr: %d m_m2_i2=%d\n", proc_nr, m.IO_ENDPT);
	
	m.POSITION = position;
	TASKDEBUG("DEV_WRITE - offset(position): m_m2_l1=%u\n", m.POSITION);
	
	char *buff; /*puntero al buffer*/
	buff = buffer;
		
	TASKDEBUG("DEV_WRITE: count %u\n", bytesCount);
	TASKDEBUG("DEV_WRITE: buff (puntero al buffer)=%p\n", buff);
	TASKDEBUG("contenido del buffer %s\n", buffer);
	m.COUNT = bytesCount; /*cantidad de bytes a escribir*/
    m.ADDRESS = buff; /*buffer del cliente*/ 

	TASKDEBUG("SENDREC msg DEV_WRITE: m_type=%d, DEVICE=%d, IO_ENDPT=%d, POSITION=%u, COUNT=%u, ADDRESS=%p\n",
           m.m_type,
           m.DEVICE,
           m.IO_ENDPT,
		   m.POSITION,
		   m.COUNT,
           m.ADDRESS);

    ret = mnx_sendrec(svr_ep, (long) &m); /*para enviar y recibir de un cliente a un servidor*/
	if( ret != 0 )
    	printf("sendrec - ret=%d\n",ret);

	sleep(2);
		
	printf("REPLY: -> m_type=%d, (REP_ENDPT)=%d, (REP_STATUS)=%d\n",
		m.m_type,
		m.REP_ENDPT,
		m.REP_STATUS);	
		
	sleep(1);
	return;
		
 }
