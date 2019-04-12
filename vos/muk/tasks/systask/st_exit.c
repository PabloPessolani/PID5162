/* The kernel call implemented in this file:
 *   m_type:	SYS_EXIT
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT		(slot number of exiting process)
 */

#include "systask.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#if USE_EXIT

/*===========================================================================*
 *				st_exit					     *
 *===========================================================================*/
int st_exit(message *m_ptr)			/* pointer to request message */
{
/* Handle sys_exit. A user process has exited or a system process requests 
 * to exit. Only the PM can request other process slots to be cleared.
 * The routine to clean up a process table slot cancels outstanding timers, 
 * possibly removes the process from the message queues, and resets certain 
 * process table fields to the default values.
 */
	proc_usr_t *proc_ptr;		/* exiting process pointer */
	priv_usr_t  *priv_ptr;
	int proc_nr, proc_ep;
  	int rcode, flags;

	proc_ep =  m_ptr->PR_ENDPT;
	MUKDEBUG("proc_ep=%d\n", proc_ep);

	proc_ptr = ENDPOINT2PTR(proc_ep);
	proc_nr = _ENDPOINT_P(proc_ep);
	
#ifdef ALLOC_LOCAL_TABLE 			
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, proc_nr, &proc_table[proc_nr+dc_ptr->dc_nr_tasks]);
	if( rcode < 0) ERROR_RETURN(rcode );
#else /* ALLOC_LOCAL_TABLE */			
	proc_ptr = (proc_usr_t *) PROC_MAPPED(proc_nr);		
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */	
	MUKDEBUG("before " PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

//	if( proc_ptr->p_rts_flags == SLOT_FREE) 
//		ERROR_RETURN(EDVSNOTBIND);
	
//	if( proc_ptr->p_nodeid != local_nodeid) 
//		ERROR_RETURN(EDVSBADNODEID);	

	/* save flags before destructive unbind */
	priv_ptr = PROC2PRIV(proc_nr);


	/* Did proc_ptr be killed by a signal sent by another process? */
	if( !TEST_BIT(proc_ptr->p_misc_flags, MIS_BIT_KILLED)){ /* NO, it exits by itself */
		SET_BIT(proc_ptr->p_misc_flags, MIS_BIT_KILLED);
		MUKDEBUG("UNBIND proc_ep=%d\n",proc_ep);
		rcode = dvk_unbind(dc_ptr->dc_dcid, proc_ep);
		if( rcode < 0 && rcode != EDVSNOTBIND) 	ERROR_RETURN(rcode);
	}else{
		MUKDEBUG("NOTIFY proc_ep=%d\n",proc_ep);
		rcode = dvk_ntfy_value(pm_ep, proc_ep, pm_ep);
		if( rcode < 0) 	ERROR_RETURN(rcode);		
#define TO_WAIT4UNBIND	100 /* miliseconds */
		MUKDEBUG("dvk_wait4unbind_T\n");
		do { 
			rcode = dvk_wait4unbind_T(proc_ep, TO_WAIT4UNBIND);
			MUKDEBUG("dvk_wait4unbind_T  rcode=%d\n", rcode);
			if (rcode == EDVSTIMEDOUT) {
				MUKDEBUG("dvk_wait4unbind_T TIMEOUT\n");
				continue ;
			}else if( rcode < 0) 
				ERROR_EXIT(EXIT_FAILURE);
		} while	(rcode < OK); 
	}
	MUKDEBUG("endpoint %d unbound\n", proc_ep);
	
		
	if( !TEST_BIT(flags, BIT_REMOTE)){	/* Local Process */	
	  	reset_timer(&priv_ptr->priv_alarm_timer);
	}

#ifdef ALLOC_LOCAL_TABLE 			
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, proc_nr, &proc_table[proc_nr+dc_ptr->dc_nr_tasks]);
	if( rcode < 0) 	ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	proc_ptr = (proc_usr_t *) PROC_MAPPED(proc_nr);	
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */		
	MUKDEBUG("after " PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	return(OK);
}


#endif /* USE_EXIT */

