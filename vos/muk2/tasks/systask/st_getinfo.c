/* The kernel call implemented in this file:
 *   m_type:	SYS_GETINFO
 *
 * The parameters for this kernel call are:
 *    m1_i3:	I_REQUEST	(what info to get)	
 *    m1_p1:	I_VAL_PTR 	(where to put it)	
 *    m1_i1:	I_VAL_LEN 	(maximum length expected, optional)	
 *    m1_p2:	I_VAL_PTR2	(second, optional pointer)	
 *    m1_i2:	I_VAL_LEN2_E	(second length or process nr)	
 */

#include "systask.h"

#undef EXTERN
#define EXTERN	extern
#include "../../glo.h"

#if USE_GETINFO

extern Task *proc_table;
extern priv_usr_t *priv_table;

/*===========================================================================*
 *			        st_getinfo				     *
 *===========================================================================*/
int st_getinfo(message *m_ptr)	/* pointer to request message */
{
/* Request system information to be copied to caller's address space. This
 * call simply copies entire data structures to the caller.
 */
  	size_t length;
  	void *src_addr; 
  	Task *proc_ptr, *lcl_ptr;
 	int ret, p_nr, p_ep, i;

MUKDEBUG("caller=%d rqst=%d addr=%X len=%d\n", 
		m_ptr->m_source, m_ptr->I_REQUEST, m_ptr->I_VAL_PTR, m_ptr->I_VAL_LEN);

	/* Set source address and length based on request type. */
  	ret = OK;
  	switch (m_ptr->I_REQUEST) {
	case GET_KINFO:  {
        	length = sizeof(dvs_usr_t);
        	src_addr = (void*)&dvs;
        	break;
    		}
    	case GET_MACHINE: {
        	length = sizeof(dc_usr_t);
        	src_addr = (void*)dc_ptr;
			MUKDEBUG("dc_id=%d \n", dc_ptr->dc_dcid);
//	SYSTASK has already get the DC info at startup
//			ret = muk_getdcinfo(dc_ptr->dc_dcid, dc_ptr);
			MUKDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
        	break;
    		}
#ifdef ANULADO			
    	case GET_PROCTAB: {
#ifdef ALLOC_LOCAL_TABLE 			
			/* it must be copied to user-space buffer because the MUK_vcopy some lines down*/
			/* is prepared for usr-usr copy */
        	length = (dvs_ptr->d_size_proc * (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)); 
			posix_memalign( (void**) &src_addr, getpagesize(), length);
			if( src_addr == NULL) return (EDVSNOMEM);
			lcl_ptr = (Task *) src_addr;
			memcpy(lcl_ptr,  get_task(-dc_ptr->dc_nr_tasks), length);
#else /* ALLOC_LOCAL_TABLE */	
        	length = (dvs_ptr->d_size_proc * (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)); 
        	src_addr = (void *) kproc_map;
#endif /* ALLOC_LOCAL_TABLE */			
        	break;
    		}
    	case GET_PRIVTAB: {
        	length = (sizeof(priv_usr_t) * (dc_ptr->dc_nr_tasks + dc_ptr->dc_nr_procs)); 
        	src_addr = (void *) priv_table;
        	break;
    		}
    	case GET_PROC: {
        	p_ep = (m_ptr->I_VAL_LEN2_E == SELF) ? m_ptr->m_source : m_ptr->I_VAL_LEN2_E;
			p_nr = _ENDPOINT_P(p_ep);
			CHECK_P_NR(p_nr); 
			MUKDEBUG("p_ep=%d p_nr=%d \n", p_ep, p_nr);
        	length = sizeof(proc_usr_t);
#ifdef ALLOC_LOCAL_TABLE 
        	src_addr = get_task(p_nr);
			proc_ptr = (Task *) src_addr;
			MUKDEBUG("BEFORE" PROC_MUK_FORMAT, PROC_MUK_FIELDS(proc_ptr));	
			ret = muk_getprocinfo(dc_ptr->dc_dcid, p_nr, src_addr);
			if( ret < 0) ERROR_RETURN(ret);
#else /* ALLOC_LOCAL_TABLE */	
			/* it must be copied to user-space buffer because the MUK_vcopy some lines down*/
			/* is prepared for usr-usr copy */
//			posix_memalign( (void**) &src_addr, getpagesize(), length);
//			if( src_addr == NULL) return (EDVSNOMEM);
			src_addr = proc_ptr = (Task *) get_task(p_nr);
			ret = OK;			
#endif /* ALLOC_LOCAL_TABLE */
			MUKDEBUG("AFTER " PROC_MUK_FORMAT, PROC_MUK_FIELDS(proc_ptr));
        	break;
    		}
#endif // ANULADO
		default:
        	ERROR_RETURN(EDVSINVAL);
 	}

	if(ret < 0 ) ERROR_RETURN(ret);
  	/* Try to make the actual copy for the requested data. */
  	if (m_ptr->I_VAL_LEN > 0 && length > m_ptr->I_VAL_LEN) ERROR_RETURN(EDVS2BIG);

//	caller_ptr = get_task(m_ptr->m_source);
//	if( caller_ptr->p_rts_flags == SLOT_FREE) 	ERROR_RETURN(EDVSNOTBIND); 
	MUK_vcopy(ret, SYSTASK(local_nodeid), src_addr, m_ptr->m_source, m_ptr->I_VAL_PTR, length);
//	memcpy( m_ptr->I_VAL_PTR, src_addr, length);

#ifdef ALLOC_LOCAL_TABLE 
	if (m_ptr->I_REQUEST == GET_PROCTAB) {
		free(src_addr);
	}
#endif /* ALLOC_LOCAL_TABLE */
	if(ret < 0) ERROR_RETURN(ret);

  return(OK);
}

#endif /* USE_GETINFO */

