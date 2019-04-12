/* The kernel call implemented in this file:
 *   m_type:	SYS_FORK
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_LPID   child Linux PID	
 * OUTPUT:
 *    m1_i2:	PR_ENDPT child endpoint
 */
#include "systask.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#if USE_FORK
#define	FORK_SLEEP_SECS		1

int get_free_proc(void);

/*===========================================================================*
 *				st_fork					     *
 *===========================================================================*/
int st_fork(message *m_ptr)	
{
/* Handle sys_fork().  PR_ENDPT has forked.  The child is PR_SLOT. */
	proc_usr_t *caller_ptr;
  proc_usr_t *child_ptr;		/* child process pointer */
  priv_usr_t *cpriv_ptr;		/* child privileges pointer */
  int child_nr, child_lpid, child_ep;
  int rcode, child_gen;

/*!!!!!!!!!!!!!! VERIFICAR QUE EL M_SOURCE SEA PM !!!!!!!!!!!!!!!!!!!!*/
//	if( parent_ptr->p_rts_flags == SLOT_FREE) 	
//		return(EDVSNOTBIND);

  	caller_ptr = PROC2PTR(sys_who_p);
	if(caller_ptr->p_endpoint != pm_ep) 
		ERROR_RETURN(EDVSACCES);

	MUKDEBUG("child_lpid=%d PMnodeid=%d\n",  m_ptr->PR_LPID, caller_ptr->p_nodeid);

	child_nr  = get_free_proc();
	if(child_nr < 0) ERROR_RETURN(child_nr);

	child_ptr = PROC2PTR(child_nr);
	child_lpid = m_ptr->PR_LPID;
	
	/* Get info about the child slot in KERNEL */	
#ifdef ALLOC_LOCAL_TABLE 			
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, child_nr, child_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	child_ptr = (proc_usr_t *) PROC_MAPPED(child_nr);
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(child_ptr));
	
	/* Test if the child was binded to SYSTASK */	
	if( child_ptr->p_rts_flags != SLOT_FREE) 	return(EDVSBUSY);
	
	/* Register the process to the kernel */
	child_gen = _ENDPOINT_G(child_ptr->p_endpoint) + 1;
	child_ep = _ENDPOINT(child_gen, child_ptr->p_nr);
	MUKDEBUG("bind dcid=%d child_lpid=%d, child_nr=%d  child_ep=%d child_gen=%d\n", 
		dc_ptr->dc_dcid, child_lpid, child_nr, child_ep, child_gen);
	rcode  = dvk_lclbind(dc_ptr->dc_dcid, child_lpid, child_ep);
	MUKDEBUG("child_ep=%d rcode=%d\n", child_ep, rcode);
	if(child_ep != rcode) ERROR_RETURN(rcode);
	
	/* Get process information from kernel */
#ifdef ALLOC_LOCAL_TABLE 			
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, child_nr, child_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	child_ptr = (proc_usr_t *) PROC_MAPPED(child_nr);	
	rcode = OK;
#endif /* ALLOC_LOCAL_TABLE */
	
	MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(child_ptr));

	/* Get process privileges from kernel */
	child_ep = child_ptr->p_endpoint;
	MUKDEBUG("child_ep=%d\n", child_ep);
	cpriv_ptr =  PROC2PRIV(child_nr);
	rcode = dvk_getpriv(dc_ptr->dc_dcid, child_ep, cpriv_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
	MUKDEBUG(PRIV_USR_FORMAT,PRIV_USR_FIELDS(cpriv_ptr));

	m_ptr->PR_ENDPT = child_ep;
	m_ptr->PR_SLOT  = child_nr;

  return(OK);
}

/*===========================================================================*
 *				get_free_proc									     *
 *===========================================================================*/
int get_free_proc(void)
{
	proc_usr_t *child_ptr;		/* child process pointer */

	/* Searches for the next free slot 	*/
	next_child = (dc_ptr->dc_nr_sysprocs);
	do { 
#ifdef ALLOC_LOCAL_TABLE 			
		child_ptr = &proc_table[next_child];
#else /* ALLOC_LOCAL_TABLE */			
		child_ptr = (proc_usr_t *) PROC_MAPPED(next_child-dc_ptr->dc_nr_tasks);						
#endif /* ALLOC_LOCAL_TABLE */		
		if( (child_ptr->p_rts_flags == SLOT_FREE) ) {
			MUKDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(child_ptr));
			MUKDEBUG("next_child=%d return=%d\n",
				next_child, next_child-dc_ptr->dc_nr_tasks);
			return(next_child-dc_ptr->dc_nr_tasks);
		}
		next_child++;
	}while(next_child < (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs));
	
	ERROR_RETURN(EDVSGENERIC);
}

#endif /* USE_FORK */

