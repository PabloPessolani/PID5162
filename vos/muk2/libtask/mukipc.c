
#include "taskimpl.h"


/*===========================================================================*
 *				muk_send				     *
 *===========================================================================*/
int muk_send(int dst_nr, message *mptr)
{
	muk_proc_t *dst_ptr, *src_ptr;
	muk_proc_t **xpp;

	int rcode;
	
	rcode = OK;
	MUKDEBUG("dst_nr:%d\n", dst_nr);
	assert(dst_nr != current_nr);
	assert(dst_nr >= (-NR_TASKS) && dst_nr < NR_PROCS);
	
	dst_ptr =get_task(dst_nr);
	if( (TEST_BIT(dst_ptr->p_rts_flags,RECEIVING) != 0) 
	 && ((dst_ptr->p_getfrom == ANY) || (dst_ptr->p_getfrom == current_nr))){
		memcpy(dst_ptr->p_msg, mptr, sizeof(message));
		dst_ptr->p_msg->m_source = current_nr;
		dst_ptr->p_getfrom = NONE;
		CLR_BIT(dst_ptr->p_rts_flags,RECEIVING);
		dst_ptr->p_error = OK;
		if( dst_ptr->p_rts_flags == 0){
			taskwakeup(dst_ptr->p_rendez);
		}		
	} else { // not receiving 
		current_ptr->p_sendto = dst_nr;
		SET_BIT(current_ptr->p_rts_flags,SENDING);
		current_ptr->p_msg = mptr;
//		TAILQ_INSERT_TAIL(&dst_ptr->p_wlist, current_ptr, p_send);	

		/* Process is now blocked.  Put in on the destination's queue. */
		xpp = &dst_ptr->p_caller_q;		/* find end of list */
		while (*xpp != NULL) xpp = &(*xpp)->p_q_link;	
		*xpp = current_ptr;			/* add caller to end */
		current_ptr->p_q_link = NULL;	/* mark new end of list */
		tasksleep(current_ptr->p_rendez);
		rcode = current_ptr->p_error;
	}
	return(rcode);
}			

/*===========================================================================*
 *				muk_receive				     *
 *===========================================================================*/
int muk_receive(int src_nr, message *mptr)
{
	muk_proc_t *src_ptr, *dst_ptr;
	muk_proc_t **xpp;
	int rcode;
	
	rcode = OK;
	MUKDEBUG("src_nr:%d\n", src_nr);
	assert(src_nr != current_nr);
	assert( (src_nr >= (-NR_TASKS) && src_nr < NR_PROCS) || (src_nr == ANY) );
	
    xpp = &current_ptr->p_caller_q;
    while (*xpp != NULL) {
		src_ptr = (*xpp);
        if (src_nr == ANY || src_nr == src_ptr->p_nr) {
			memcpy(mptr, src_ptr->p_msg, sizeof(message));
			src_ptr->p_sendto = NONE;
			src_ptr->p_error = OK;
			CLR_BIT(src_ptr->p_rts_flags, SENDING);
			*xpp = src_ptr->p_q_link;
			if(src_ptr->p_rts_flags == 0)
				taskwakeup(src_ptr->p_rendez);
            return(OK);				/* report success */
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

	SET_BIT(current_ptr->p_rts_flags,RECEIVING);
	current_ptr->p_getfrom = src_nr;
	tasksleep(current_ptr->p_rendez);
	rcode = current_ptr->p_error;
	return(rcode);
}	

