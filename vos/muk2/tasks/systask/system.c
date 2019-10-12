/****************************************************************/
/* 				SYSTASK				*/
/* SYSTASK is like the SYSTEM TASK of MINIX			*/
/****************************************************************/

#define DVK_GLOBAL_HERE
#define _SYSTEM
#include "systask.h"

#undef DVK_GLOBAL_HERE
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#define TRUE 1

/* Declaration of the call vector that defines the mapping of system calls 
 * to handler functions. The vector is initialized in sys_init() with map(), 
 * which makes sure the system call numbers are ok. No space is allocated, 
 * because the dummy is declared extern. If an illegal call is given, the 
 * array size will be negative and this won't compile. 
 */

int (*call_vec[NR_SYS_CALLS])(message *m_ptr);

#define map(call_nr, handler) call_vec[(call_nr-KERNEL_CALL)] = (handler)

int init_systask(int dcid);

/****************************************************************************************/
/*			SYSTEM TASK 						*/
/****************************************************************************************/
int  main_systask ( int argc, char *argv[] )
{
	int rcode;
	unsigned int call_nr;
	int result;
	Task *task_ptr;
	proc_usr_t *proc_ptr;
	message *m_ptr;
	
	pagesize = sysconf(_SC_PAGESIZE); /*  getpagesize() */

  	/* Initialize the system task. */
	rcode = init_systask(dcid);
	if(rcode < 0) ERROR_EXIT(rcode);

	rcode = init_clock(dcid);
	if (rcode ) ERROR_EXIT(rcode);

	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(sys_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);
	
	/*------------------------------------
	 * SYSTEM TASK LOOP
	 *------------------------------------*/
	while(TRUE) {

		/*------------------------------------
		* Receive Request 
	 	*------------------------------------*/
		MUKDEBUG("SYSTASK is waiting for requests\n");
		rcode = muk_receive(ANY,&sys_m);

		MUKDEBUG("muk_receive rcode=%d\n", rcode);
		if(rcode < 0 ) {
			ERROR_PRINT(rcode);
			taskexit(&rcode);
		}

		m_ptr = &sys_m;
   		MUKDEBUG("RECEIVE msg:"MSG4_FORMAT,MSG4_FIELDS(m_ptr));
		
		/*------------------------------------
		* Process the Request 
	 	*------------------------------------*/

		call_nr = (unsigned) m_ptr->m_type - KERNEL_CALL;	
      	sys_who_e = m_ptr->m_source;
		sys_who_p = _ENDPOINT_P(sys_who_e);
		MUKDEBUG("call_nr=%d sys_who_e=%d\n", call_nr, sys_who_e);
		if (call_nr >= NR_SYS_CALLS || call_nr < 0) {	/* check call number 	*/
			ERROR_PRINT(EDVSBADREQUEST);
			result = EDVSBADREQUEST;		/* illegal message type */
      	} else {	
			MUKDEBUG("Calling vector %d\n",call_nr);
//while(1) sleep(1);
//exit(1);	
        	result = (*call_vec[call_nr])(&sys_m);	/* handle the system call*/
      	}
			
		/*------------------------------------
	 	* Send Reply 
 		*------------------------------------*/
		if (result != EDVSDONTREPLY) {
  	  		m_ptr->m_type = result;		/* report status of call */
			MUKDEBUG("REPLY msg:"MSG1_FORMAT,MSG1_FIELDS(m_ptr));
    		rcode = muk_send(sys_m.m_source, (long) &sys_m);
			if(rcode < 0) {
				switch(rcode){  /* Auto Unbind the failed process */
					case	EDVSSRCDIED:
					case	EDVSDSTDIED:
					case	EDVSNOPROXY:
					case	EDVSNOTCONN:
					case	EDVSDEADSRCDST:
						proc_ptr = task_ptr->p_proc;
#ifdef ALLOC_LOCAL_TABLE 			
						rcode = muk_getprocinfo(dc_ptr->dc_dcid, sys_m.m_source, task_ptr);	
						if( rcode < 0) ERROR_RETURN(rcode );
#else /* ALLOC_LOCAL_TABLE */			
						task_ptr = (Task *) get_task(sys_m.m_source);
						rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */			
						
						if( proc_ptr->p_rts_flags == SLOT_FREE ) break;					
						rcode = muk_unbind(dc_ptr->dc_dcid, sys_m.m_source);
						if( rcode < 0) ERROR_RETURN(rcode );
#ifdef ALLOC_LOCAL_TABLE 			
						rcode = muk_getprocinfo(dc_ptr->dc_dcid, sys_m.m_source, task_ptr);
						if( rcode < 0) ERROR_RETURN(rcode );
#endif /* ALLOC_LOCAL_TABLE */			
						break;
					default:
						break;
				}
				if( rcode < 0) ERROR_PRINT(rcode);
				continue;
			}
		}
	}		

	if(rcode < 0) ERROR_RETURN(rcode);
	return(OK);
 }


/*===========================================================================*
 *				init_systask				     *
 *===========================================================================*/
int init_systask(int dcid)
{
	proc_usr_t *proc_ptr;
	int i, rcode;
static char debug_path[MNX_PATH_MAX];

	sys_id = taskid();
	MUKDEBUG("sys_id=%d sys_ep=%d\n", sys_id, SYSTASK(local_nodeid));
	
	sys_ep = muk_tbind(dcid,SYSTASK(local_nodeid),"systask");
	MUKDEBUG("sys_ep=%d\n", sys_ep);
	if( sys_ep != SYSTASK(local_nodeid)) {
		ERROR_PRINT(EDVSENDPOINT);
		taskexit(&sys_ep);
	}
	sys_ptr = current_task();
	proc_ptr = sys_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));

	last_rqst = time(NULL);

  /* Initialize the call vector to a safe default handler. Some system calls 
   * may be disabled or nonexistant. Then explicitely map known calls to their
   * handler functions. This is done with a macro that gives a compile error
   * if an illegal call number is used. The ordering is not important here.
   */
  	MUKDEBUG( "Initialize the call vector to a safe default handler.\n");
  	for (i=0; i<NR_SYS_CALLS; i++) {
//		MUKDEBUG("Initilizing vector %d on address=%p\n",i, st_unused);
      		call_vec[i] = st_unused;
  	}

  /* Process management. */
    map(SYS_FORK, st_fork); 		/* a process forked a new process */
//  map(SYS_EXEC, do_exec);		/* update process after execute */
    map(SYS_EXIT, st_exit);		/* clean up after process exit */
    map(SYS_NICE, st_nice);		/* set scheduling priority */
    map(SYS_PRIVCTL, st_privctl);	/* system privileges control */
//  map(SYS_TRACE, do_trace);		/* request a trace operation */

  /* Signal handling. */
//  map(SYS_KILL, do_kill); 		/* cause a process to be signaled */
//  map(SYS_GETKSIG, do_getksig);	/* PM checks for pending signals */
//  map(SYS_ENDKSIG, do_endksig);	/* PM finished processing signal */
//  map(SYS_SIGSEND, do_sigsend);	/* start POSIX-style signal */
//  map(SYS_SIGRETURN, do_sigreturn);	/* return from POSIX-style signal */

  /* Device I/O. */
//  map(SYS_IRQCTL, do_irqctl);  	/* interrupt control operations */ 
//  map(SYS_DEVIO, do_devio);   	/* inb, inw, inl, outb, outw, outl */ 
//  map(SYS_SDEVIO, do_sdevio);		/* phys_insb, _insw, _outsb, _outsw */
//  map(SYS_VDEVIO, do_vdevio);  	/* vector with devio requests */ 
//  map(SYS_INT86, do_int86);  		/* real-mode BIOS calls */ 

  /* Memory management. */
//  map(SYS_NEWMAP, do_newmap);		/* set up a process memory map */
//  map(SYS_SEGCTL, do_segctl);		/* add segment and get selector */
    map(SYS_MEMSET, st_memset);		/* write char to memory area */
//  map(SYS_dc_SETBUF, do_dc_setbuf); 	/* PM passes buffer for page tables */
//  map(SYS_dc_MAP, do_dc_map); 	/* Map/unmap physical (device) memory */

  /* Copying. */
//  map(SYS_UMAP, do_umap);		/* map virtual to physical address */
  map(SYS_VIRCOPY, st_copy); 	/* use pure virtual addressing */
  map(SYS_PHYSCOPY, st_copy); 	/* use physical addressing */
  map(SYS_VIRVCOPY, st_copy);	/* vector with copy requests */
  map(SYS_PHYSVCOPY, st_copy);	/* vector with copy requests */

  /* Clock functionality. */
    map(SYS_TIMES, st_times);		/* get uptime and process times */
    map(SYS_SETALARM, st_setalarm);	/* schedule a synchronous alarm */

  /* System control. */
//  map(SYS_ABORT, do_abort);		/* abort MINIX */
    map(SYS_GETINFO, st_getinfo); 	/* request system information */ 
//  map(SYS_IOPENABLE, do_iopenable); 	/* Enable I/O */

   	map(SYS_KILLED, st_killed);
	map(SYS_BINDPROC, st_bindproc);	
//	map(SYS_REXEC, do_rexec);	
//	map(SYS_MIGRPROC, do_migrproc);	
	map(SYS_SETPNAME, st_setpname);	
	map(SYS_UNBIND, st_unbind);	
	map(SYS_DUMP, st_dump);	

	dc_ptr = &dcu;

	return(OK);
}



