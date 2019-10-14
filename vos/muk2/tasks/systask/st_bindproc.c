/* 
The kernel call implemented in this file:
 *   m_type:	SYS_BINDPROC
 *
 * The parameters for this kernel call are:
 *    PR_SLOT	 (child's process table slot)	
 *    PR_LPID   child Linux PID	
 
The kernel call implemented in this file:
 *   m_type:	SYS_UNBIND
 *
 * The parameters for this kernel call are:
 *    PR_ENDPT	

 */
#include "systask.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#if USE_BINDPROC

/*===========================================================================*
 *				st_bindproc				     *
 * A System process:
 *		- SERVER/TASK: is just registered to the kernel but not to SYSTASK.
 *		- REMOTE USER: it will be registered to the kernel and then to SYSTASK 
 * INPUT: M3 type message
 *   	M3_ENDPT
 *      M3_LPID (PID or NODEID)
 * 	M3_OPER must be  (minix/com.h)
 * 		SELF_BIND	0: pid, local_nodeid, self endpoint
 * 		LCL_BIND		1: pid, local_nodeid, self endpoint
 * 		RMT_BIND	2: nodeid
 * 		BKUP_BIND	3: pid, local_nodeid, self endpoint
 * 		REPLICA_BIND 4
 *     m3_ca1: Process namespace
 *
 * OUTPUT: M1 type message
 *       m_type: rcode
 *       M1_ENDPT
 *       
 *===========================================================================*/
int st_bindproc(message *m_ptr)	
{
  Task *sysproc_ptr;		/* sysproc process pointer */
  int sysproc_nr, sysproc_lpid, sysproc_ep, sysproc_oper, sysproc_nodeid;
  int rcode;
    proc_usr_t *proc_ptr;


	MUKDEBUG(MSG3_FORMAT,  MSG3_FIELDS(m_ptr));
	sysproc_ep 		= m_ptr->M3_ENDPT;
	sysproc_nr		= _ENDPOINT_P(sysproc_ep);
	sysproc_nodeid  = sysproc_lpid = m_ptr->M3_LPID;
	sysproc_oper	= (int) m_ptr->M3_OPER; 
	MUKDEBUG("sysproc_lpid=%d, sysproc_ep=%d sysproc_nr=%d sysproc_oper=%X \n", 
		sysproc_lpid, sysproc_ep, sysproc_nr, sysproc_oper);
	if( sysproc_oper < SELF_BIND || sysproc_oper > REPLICA_BIND) 
		ERROR_RETURN(EDVSBADRANGE);
	
	CHECK_P_NR(sysproc_nr);		
	
	/* GET process information from kernel */
#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, sysproc_nr, sysproc_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	sysproc_ptr = (Task *) get_task(sysproc_ep);
	rcode = OK;	
#endif /* ALLOC_LOCAL_TABLE */
	proc_ptr = sysproc_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	
	/* REMOTE USER PROCESSES */
#ifdef SUPPORT_REMOTE	
	if( (sysproc_nr+dc_ptr->dc_nr_tasks) >= dc_ptr->dc_nr_sysprocs) {
		/* user processes can be bound only if they are REMOTE */
		if( sysproc_oper != RMT_BIND )
			ERROR_RETURN(EDVSPERM);
		/* The NODE must other than local node  */
		if( sysproc_nodeid == local_nodeid) 
			ERROR_RETURN(EDVSBADNODEID);
		if(  TEST_BIT(sysproc_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) {
			/* Any process is bound to this slot */
			MUKDEBUG("Any process is bound to endpoint=%d (new is %s)\n", 
				sysproc_ep, m_ptr->m3_ca1 );
			rcode = muk_rmtbind(dc_ptr->dc_dcid,m_ptr->m3_ca1,
						sysproc_ep,sysproc_nodeid);
			if( rcode < 0) ERROR_RETURN(rcode);
		}else{
			/* A LOCAL process is already bound to this slot */
			if( sysproc_ptr->p_proc->p_nodeid == local_nodeid)
				MUKDEBUG("A LOCAL process is already bound to this slot:%s\n",
					sysproc_ptr->p_name);
				ERROR_RETURN(EDVSSLOTUSED);
			/* A REMOTE process is already bound to this slot */			
			if(sysproc_ptr->p_proc->p_endpoint == sysproc_ep){
				MUKDEBUG("A REMOTE process is already bound to this slot:%s\n",
					sysproc_ptr->p_name);
				/* The slot has same endpoint */
				if(sysproc_ptr->p_proc->p_nodeid != sysproc_nodeid){
					/* the process also has different nodeid : MIGRATE IT */
					MUKDEBUG("the process also has different nodeid %d: MIGRATE IT \n",
						sysproc_ptr->p_proc->p_nodeid);
					muk_migr_start(dc_ptr->dc_dcid, sysproc_ptr->p_proc->p_endpoint);
					muk_migr_commit(PROC_NO_PID, dc_ptr->dc_dcid, 
						sysproc_ep, sysproc_nodeid);			
				}else{
					/* same node, same endpoint */
					MUKDEBUG("Remote process already bound\n");
					proc_ptr = sysproc_ptr->p_proc;
					MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
					m_ptr->M1_ENDPT = sysproc_ep;
					m_ptr->M1_OPER  = sysproc_oper;
				}
			} else{
				/*Another Remote process was using the slot: Unbind it before new bind */
				MUKDEBUG("Another Remote process was using the slot: Unbind it before new bind %s\n",
						sysproc_ptr->p_name);						
				muk_unbind(dc_ptr->dc_dcid,sysproc_ptr->p_proc->p_endpoint);
				rcode = muk_rmtbind(dc_ptr->dc_dcid,basename(m_ptr->m3_ca1),
							sysproc_ep,sysproc_nodeid);
				if( rcode < 0) ERROR_RETURN(rcode);
			}
		}
#ifdef ALLOC_LOCAL_TABLE 			
		rcode = muk_getprocinfo(dc_ptr->dc_dcid, sysproc_nr, sysproc_ptr);
		if( rcode < 0) ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
		sysproc_ptr = (Task *) get_task(sysproc_nr);	
		rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */
		proc_ptr = sysproc_ptr->p_proc;
		MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
		return(OK);
	}

#endif // SUPPORT_REMOTE	

#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, sysproc_nr, sysproc_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	sysproc_ptr = (Task *) get_task(sysproc_nr);	
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */
	proc_ptr = sysproc_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	
	if(  TEST_BIT(sysproc_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) 
		ERROR_RETURN(EDVSNOTBIND);
		
//	if( sysproc_ptr->p_proc->p_nodeid == local_nodeid) 
//		strncpy(sysproc_ptr->p_name, basename(m_ptr->M3_NAME),(M3_STRING-1));
	proc_ptr = sysproc_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
		
	m_ptr->M1_ENDPT = sysproc_ep;
	m_ptr->M1_OPER  = sysproc_oper;

	return(OK);
}

#endif /* USE_BINDPROC */

#if USE_SETPNAME
int st_setpname(message *m_ptr)	
{
	Task *sysproc_ptr;		/* sysproc process pointer */
	int sysproc_nr, sysproc_ep;
    proc_usr_t *proc_ptr;

	MUKDEBUG(MSG3_FORMAT,  MSG3_FIELDS(m_ptr));
	sysproc_ep 		= m_ptr->M3_ENDPT;
	sysproc_nr		= _ENDPOINT_P(sysproc_ep);
	
	CHECK_P_NR(sysproc_nr);		
	sysproc_ptr 	= get_task(sysproc_nr);
	
	/* GET process information from kernel */
#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, sysproc_nr, sysproc_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	sysproc_ptr = (Task *) get_task(sysproc_nr);
#endif /* ALLOC_LOCAL_TABLE */
	proc_ptr = sysproc_ptr->p_proc;
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	
	if(  TEST_BIT(sysproc_ptr->p_proc->p_rts_flags, BIT_SLOT_FREE)) 
		ERROR_RETURN(EDVSNOTBIND);
	
	strncpy(sysproc_ptr->name, m_ptr->M3_NAME,(M3_STRING-1));
	
	return(OK);
}
#endif /* USE_SETPNAME */

#if USE_UNBIND
/*===========================================================================*
 *				st_unbind 					     *
 *===========================================================================*/
int st_unbind(message *m_ptr)			/* pointer to request message */
{

	Task *task_ptr;		/* exiting process pointer */
	int proc_nr, proc_ep;
  	int rcode, flags;
    proc_usr_t *proc_ptr;


	proc_ep =  m_ptr->PR_ENDPT;
	MUKDEBUG("proc_ep=%d\n", proc_ep);

	task_ptr = get_task(proc_ep);
	proc_nr = _ENDPOINT_P(proc_ep);
	
#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, proc_nr, &proc_table[proc_nr+dc_ptr->dc_nr_tasks]);
	if( rcode < 0) ERROR_RETURN(rcode );
#else /* ALLOC_LOCAL_TABLE */			
	task_ptr = (Task *) get_task(proc_nr);		
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */	
	proc_ptr = task_ptr->p_proc;
	MUKDEBUG("before " PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	
	if( task_ptr->p_proc->p_rts_flags == SLOT_FREE) 
		ERROR_RETURN(EDVSNOTBIND);
	
	/* save flags before destructive unbind */
	flags = task_ptr->p_proc->p_rts_flags;

	/* Did task_ptr be killed by a signal sent by another process? */
	if( !TEST_BIT(task_ptr->p_proc->p_misc_flags, MIS_BIT_KILLED)){ /* NO, it exits by itself */
		SET_BIT(task_ptr->p_proc->p_misc_flags, MIS_BIT_KILLED);
		MUKDEBUG("UNBIND proc_ep=%d\n",proc_ep);
		rcode = muk_unbind(dc_ptr->dc_dcid, proc_ep);
		if( rcode < 0) 	ERROR_RETURN(rcode);
	}else{
		MUKDEBUG("NOTIFY proc_ep=%d\n",proc_ep);
		rcode = muk_src_notify(pm_ep, proc_ep);
		if( rcode < 0) 	ERROR_RETURN(rcode);		
#define TO_WAIT4UNBIND	100 /* miliseconds */
		MUKDEBUG("muk_wait4unbind_T\n");
		do { 
			rcode = muk_wait4unbind_T(proc_ep, TO_WAIT4UNBIND);
			MUKDEBUG("muk_wait4unbind_T  rcode=%d\n", rcode);
			if (rcode == EDVSTIMEDOUT) {
				MUKDEBUG("muk_wait4unbind_T TIMEOUT\n");
				continue ;
			}else if( rcode < 0) 
				ERROR_EXIT(EXIT_FAILURE);
		} while	(rcode < OK); 
	}
	MUKDEBUG("endpoint %d unbound\n", proc_ep);
			
	if( !TEST_BIT(flags, BIT_REMOTE)){	/* Local Process */	
	  	reset_timer(&task_ptr->p_alarm_timer);
	}

#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, proc_nr, &proc_table[proc_nr+dc_ptr->dc_nr_tasks]);
	if( rcode < 0) 	ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	task_ptr = (Task *) get_task(proc_nr);	
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */	
	proc_ptr = task_ptr->p_proc;
	MUKDEBUG("after " PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	
	return(OK);
}
#endif // USE_UNBIND


