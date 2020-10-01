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
	proc_usr_t *proc_ptr;
	message *m_ptr;
	
	pagesize = sysconf(_SC_PAGESIZE); /*  getpagesize() */

  	/* Initialize the system task. */
	rcode = init_systask(dcid);
	if(rcode < 0) ERROR_EXIT(rcode);

	rcode = init_clock(dcid);
	if (rcode ) ERROR_EXIT(rcode);

	// SYNCHRONIZE WITH MUK
	MTX_LOCK(sys_mutex);
	MTX_UNLOCK(muk_mutex);
	
	/*------------------------------------
	 * SYSTEM TASK LOOP
	 *------------------------------------*/
	while(TRUE) {

		/*------------------------------------
		* Receive Request 
	 	*------------------------------------*/
		MUKDEBUG("SYSTASK is waiting for requests\n");
//		rcode = dvk_rcvrqst(&sys_m);
		rcode = dvk_receive(ANY,&sys_m);

		MUKDEBUG("dvk_receive rcode=%d\n", rcode);
		if(rcode < 0 ) {
			SYSERR(rcode);
//			continue;
			pthread_exit(&rcode);
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
			SYSERR(EDVSBADREQUEST);
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
    		rcode = dvk_send(sys_m.m_source, (long) &sys_m);
//    			rcode = dvk_send(sys_m.m_source, (long) &sys_m);
			if(rcode < 0) {
				switch(rcode){  /* Auto Unbind the failed process */
					case	EDVSSRCDIED:
					case	EDVSDSTDIED:
					case	EDVSNOPROXY:
					case	EDVSNOTCONN:
					case	EDVSDEADSRCDST:
#ifdef ALLOC_LOCAL_TABLE 			
						rcode = dvk_getprocinfo(dc_ptr->dc_dcid, sys_m.m_source, proc_ptr);	
						if( rcode < 0) ERROR_RETURN(rcode );
#else /* ALLOC_LOCAL_TABLE */			
						proc_ptr = (proc_usr_t *) PROC_MAPPED(_ENDPOINT_P(sys_m.m_source));
						rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */			
						
						if( proc_ptr->p_rts_flags == SLOT_FREE ) break;					
						rcode = dvk_unbind(dc_ptr->dc_dcid, sys_m.m_source);
						if( rcode < 0) ERROR_RETURN(rcode );
#ifdef ALLOC_LOCAL_TABLE 			
						rcode = dvk_getprocinfo(dc_ptr->dc_dcid, sys_m.m_source, proc_ptr);
						if( rcode < 0) ERROR_RETURN(rcode );
#endif /* ALLOC_LOCAL_TABLE */			
						break;
					default:
						break;
				}
				if( rcode < 0) SYSERR(rcode);
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
	priv_usr_t *priv_ptr;
	proc_usr_t *proc_ptr;
	int i, rcode;
static char debug_path[MNX_PATH_MAX];

	sys_pid = syscall (SYS_gettid);
	MUKDEBUG("sys_pid=%d\n", sys_pid);
	
	sys_ep = dvk_tbind(dcid,SYSTASK(local_nodeid));
	MUKDEBUG("sys_ep=%d errno=%d\n", sys_ep, errno);
	if( sys_ep != SYSTASK(local_nodeid)) {
		ERROR_PRINT(EDVSENDPOINT);
		pthread_exit(&sys_ep);
	}
	
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

	/*------------------------------------
	* Alloc memory for process table 
 	*------------------------------------*/
#ifdef ALLOC_LOCAL_TABLE	
//	proc_table = (proc_usr_t *) malloc(sizeof(proc_usr_t) * (dcu.dc_nr_procs+dcu.dc_nr_tasks));
	posix_memalign( (void**) &proc_table, getpagesize(), sizeof(proc_usr_t) * (dcu.dc_nr_procs+dcu.dc_nr_tasks));
	if( proc_table == NULL) ERROR_RETURN (EDVSNOMEM);
  	MUKDEBUG( "Allocated space for nr_procs=%d + nr_tasks=%d processes\n",
		dcu.dc_nr_procs,dcu.dc_nr_tasks); 
#else /* ALLOC_LOCAL_TABLE */	
	/*------------------------------------
	* Map kernel process table 
 	*------------------------------------*/
	sprintf( debug_path, "/sys/kernel/debug/dvs/%s/procs",dc_ptr->dc_name);
  	MUKDEBUG( "Map kernel process table on %s\n",debug_path);
	debug_fd = open(debug_path, O_RDWR);
	if(debug_fd < 0) {
		fprintf(stderr, "%s\n", *strerror(errno));
		ERROR_EXIT(errno);
	}
  	MUKDEBUG("open debug_fd=%d\n",debug_fd);

	kproc_map = mmap(NULL, ((dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs) * dvs_ptr->d_size_proc) 
					, (PROT_READ | PROT_WRITE) , MAP_SHARED, debug_fd, 0);
	if (kproc_map == MAP_FAILED) {
		MUKDEBUG("mmap %d\n",errno);
		fprintf(stderr, "mmap %d\n",errno);
		ERROR_EXIT(-errno);
	}	
		
	for (i = -dc_ptr->dc_nr_tasks; i < (dc_ptr->dc_nr_procs); i++) {
		proc_ptr = (proc_usr_t *) PROC_MAPPED(i);
		MUKDEBUG("proc_ptr=%x " PROC_USR_FORMAT, proc_ptr, PROC_USR_FIELDS(proc_ptr));
	}
#endif /* ALLOC_LOCAL_TABLE */	
		
	/*------------------------------------
	* Alloc memory for priv_table table 
 	*------------------------------------*/
//	priv_table = (priv_usr_t *) malloc(sizeof(priv_usr_t) * (dcu.dc_nr_procs+dcu.dc_nr_tasks));
	posix_memalign( (void**) &priv_table, getpagesize(), sizeof(priv_usr_t) * (dcu.dc_nr_procs+dcu.dc_nr_tasks));
	if( priv_table == NULL) return (EDVSNOMEM);
  	MUKDEBUG( "Allocated space for nr_procs=%d + nr_tasks=%d ptrivileges\n",
		dcu.dc_nr_procs,dcu.dc_nr_tasks); 

	/*------------------------------------
	* Initialize priv_table table 
 	*------------------------------------*/
	MUKDEBUG("Initialize priv_table table\n");
	for (i = 0; i < (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs); i++) {
		priv_ptr = &priv_table[i];
		rcode  = dvk_getpriv(dcu.dc_dcid, (i-dc_ptr->dc_nr_tasks), priv_ptr);
		if( rcode < 0 && rcode != EDVSDSTDIED ) ERROR_RETURN(rcode);		
	}

	/*------------------------------------
	* Set SYSTASK/CLOCK privileges 
 	*------------------------------------*/
	priv_ptr = &priv_table[SYSTEM+dc_ptr->dc_nr_tasks];
	priv_ptr->priv_level = SYSTEM_PRIV;
	priv_ptr->priv_warn = NONE;
	rcode  = dvk_setpriv(dcu.dc_dcid, SYSTEM, priv_ptr);
	if( rcode < 0 && rcode != EDVSDSTDIED ) ERROR_RETURN(rcode);		

	rcode = setpriority(PRIO_PROCESS, getpid(), PRIO_SYSTASK); 
	if( rcode < 0) ERROR_RETURN(rcode);		

	return(OK);
}



