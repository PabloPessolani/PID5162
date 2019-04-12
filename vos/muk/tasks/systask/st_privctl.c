/* The kernel call implemented in this file:
 *   m_type:	SYS_PRIVCTL
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT 	(process number of caller)	
 *    m1_i2:	PR_PRIV 	(process privilege level )	
*/
#include "systask.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#if USE_PRIVCTL


/*===========================================================================*
 *				st_privctl				     *
 *===========================================================================*/
int st_privctl(message *m_ptr)			/* pointer to request message */
{
/* Handle sys_privctl(). Update a process' privileges. If the process is not
 * yet a system process, make sure it gets its own privilege structure.
 */
  proc_usr_t *caller_ptr, *proc_ptr;
  priv_usr_t *cpriv_ptr, *ppriv_ptr;
  int caller_ep, proc_ep;
  int proc_nr, caller_nr;
  int level, rcode;

	MUKDEBUG("caller_ep=%d proc_ep=%d priv_table=%d\n", 
		sys_who_p, m_ptr->PR_ENDPT, m_ptr->PR_PRIV);

	caller_ep = sys_who_p;
	caller_nr = _ENDPOINT_P(caller_ep);
  	caller_ptr = PROC2PTR(sys_who_p);
  	cpriv_ptr  = PROC2PRIV(sys_who_p);

  	proc_ep =  m_ptr->PR_ENDPT;
	proc_nr = _ENDPOINT_P(proc_ep);
	proc_ptr = PROC2PTR(proc_nr);
	MUKDEBUG("proc_ep=%d proc_nr=%d\n", proc_ep, proc_nr);

#ifdef ALLOC_LOCAL_TABLE 			
	rcode = dvk_getprocinfo(dc_ptr->dc_dcid, proc_nr, proc_ptr);
	if( rcode < 0) 	ERROR_RETURN(rcode);
#else /* ALLOC_LOCAL_TABLE */			
	proc_ptr = (proc_usr_t *) PROC_MAPPED(proc_nr);	
	rcode = OK;	
#endif /* ALLOC_LOCAL_TABLE */
	if( proc_ptr->p_rts_flags == SLOT_FREE) 	ERROR_RETURN(EDVSNOTBIND); 
  	ppriv_ptr = PROC2PRIV(proc_nr);

	rcode = dvk_getpriv(dcu.dc_dcid, proc_ep, ppriv_ptr);
	if( rcode < 0) 	ERROR_RETURN(rcode);
	MUKDEBUG("OLD " PRIV_USR_FORMAT, PRIV_USR_FIELDS(ppriv_ptr));

	if( (caller_nr != pm_ep )	
		&& (caller_nr != DS_PROC_NR ) ) /* ONLY FOR TESTING */
			ERROR_RETURN(EDVSPERM);	
 
// HABILITAR  if( (proc_nr != pm_ep) && (cpriv_ptr < ppriv_ptr)) ERROR_RETURN(EDVSPERM);
	
	level = m_ptr->PR_PRIV;
	if( (level < USER_PRIV) || (level > KERNEL_PRIV)) ERROR_RETURN(EDVSINVAL); 
 	ppriv_ptr->priv_level = level;
	rcode = dvk_setpriv(dcu.dc_dcid, proc_ep, ppriv_ptr);
	if( rcode < 0) 	ERROR_RETURN(rcode);

	rcode = dvk_getpriv(dcu.dc_dcid, proc_ep, ppriv_ptr);
	if( rcode < 0) 	ERROR_RETURN(rcode);
	MUKDEBUG("NEW " PRIV_USR_FORMAT, PRIV_USR_FIELDS(ppriv_ptr));
	
	return(OK);
}

#endif /* USE_PRIVCTL */

