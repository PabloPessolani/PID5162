/* The kernel call implemented in this file:
 *   m_type:	SYS_NICE
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT   	process number to change priority
 *    m1_i2:	PR_PRIORITY	the new priority
 */

#include "systask.h"
#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#if USE_NICE

/*===========================================================================*
 *				  st_nice				     *
 *===========================================================================*/
int st_nice(message *m_ptr)
{
/* Change process priority or stop the process. */
  Task *proc_ptr;		/* process pointer */
  int proc_nr, proc_ep;
  int rcode, prio;


	proc_ep =  m_ptr->PR_ENDPT;
	prio =  m_ptr->PR_PRIORITY;

	proc_nr = _ENDPOINT_P(proc_ep);
	proc_ptr = get_task(proc_ep);
	MUKDEBUG("proc_ep=%d prio=%d id=%d\n", proc_ep, prio, proc_ptr->id);

#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, proc_nr, &proc_table[proc_nr+dc_ptr->dc_nr_tasks]);
	if( rcode < 0) 	ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	proc_ptr = (Task *) get_task(proc_ep);
	rcode = OK;	
#endif /* ALLOC_LOCAL_TABLE */
	MUKDEBUG("before " PROC_MUK_FORMAT,PROC_MUK_FIELDS(proc_ptr));

	if( proc_ptr->p_rts_flags == SLOT_FREE) 	ERROR_RETURN(EDVSNOTBIND); 
	if( (proc_ptr->p_nr+ dc_ptr->dc_nr_tasks)  < dc_ptr->dc_nr_sysprocs) 	ERROR_RETURN(EDVSPERM); 
	
//	if (prio == PRIO_STOP) {
//	      	/* Take process off the scheduling queues. */
//     		proc_ptr->p_rts_flags |= NO_PRIORITY;
//     		return(OK);
//  	}

	MUKDEBUG("PRIO_MIN=%d PRIO_MAX=%d\n", PRIO_MIN, PRIO_MAX);

	if( prio < PRIO_MIN || prio > PRIO_MAX)	ERROR_RETURN(EDVSINVAL);

	rcode = setpriority(PRIO_PROCESS, proc_ptr->p_id, prio); 
	if( rcode < 0) 	ERROR_RETURN(rcode);

#ifdef ALLOC_LOCAL_TABLE 			
	rcode = muk_getprocinfo(dc_ptr->dc_dcid, proc_nr, &proc_table[proc_nr+dc_ptr->dc_nr_tasks]);
	if( rcode < 0) 	ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	proc_ptr = (Task *) get_task(proc_ep);
	rcode = OK;	
#endif /* ALLOC_LOCAL_TABLE */
	MUKDEBUG("after " PROC_MUK_FORMAT,PROC_MUK_FIELDS(proc_ptr));

	prio = getpriority(PRIO_PROCESS, proc_ptr->p_id);
	if( prio == (-1) && errno != 0) ERROR_RETURN(errno);

	MUKDEBUG("Priority of %d=%d\n",proc_ptr->p_id, prio );
	m_ptr->PR_PRIORITY=prio;

  return(OK);
}

#endif /* USE_NICE */

