
#include "taskimpl.h"


/*===========================================================================*
 *				muk_send				     *
 *===========================================================================*/
int muk_send(int dst_ep, message *mptr)
{
	muk_proc_t *dst_ptr, *src_ptr;
	muk_proc_t **xpp;
	muk_proc_t *current_ptr;
	int rcode, current_nr, current_ep;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_nr;
	current_ep = current_ptr->p_endpoint;
	LIBDEBUG("dst_ep:%d current_ep=%d\n", dst_ep, current_ep);
	assert(dst_ep != current_ep);
	assert(dst_ep >= (-NR_TASKS) && dst_ep < NR_PROCS);
	dst_ptr =get_task(_ENDPOINT_P(dst_ep));

	if( (TEST_BIT(dst_ptr->p_rts_flags,RECEIVING) != 0) 
	 && ((dst_ptr->p_getfrom == ANY) || (dst_ptr->p_getfrom == current_ep))){
		LIBDEBUG("dst_ep:%d WAITING FOR THIS MESSAGE\n", dst_ep);
		memcpy(dst_ptr->p_msg, mptr, sizeof(message));
		dst_ptr->p_msg->m_source = current_ep;
		dst_ptr->p_getfrom = NONE;
		CLR_BIT(dst_ptr->p_rts_flags,RECEIVING);
		dst_ptr->p_error = OK;
		LIBDEBUG("p_rts_flags:%lX\n", dst_ptr->p_rts_flags );
		if( dst_ptr->p_rts_flags == 0){
			taskwakeup(&dst_ptr->p_rendez);
		}		
	} else { // not receiving 
		LIBDEBUG("dst_ep:%d NOT RECEIVENG\n", dst_ep);
		current_ptr->p_sendto = dst_ep;
		SET_BIT(current_ptr->p_rts_flags,SENDING);
		current_ptr->p_msg = mptr;
		current_ptr->p_msg->m_source = current_ep;
		/* Process is now blocked.  Put in on the destination's queue. */
		xpp = &dst_ptr->p_caller_q;		/* find end of list */
		while (*xpp != NULL) xpp = &(*xpp)->p_q_link;	
		*xpp = current_ptr;			/* add caller to end */
		current_ptr->p_q_link = NULL;	/* mark new end of list */
		tasksleep(&current_ptr->p_rendez);
		rcode = current_ptr->p_error;
	}
	return(rcode);
}			

/*===========================================================================*
 *				muk_receive				     *
 *===========================================================================*/
int muk_receive(int src_ep, message *mptr)
{
	muk_proc_t *src_ptr, *dst_ptr;
	muk_proc_t **xpp;
	int i, p_nr, src_nr;
	muk_proc_t *current_ptr;
	int rcode, current_nr, current_ep;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_nr;
	current_ep = current_ptr->p_endpoint;
	
	LIBDEBUG("src_ep:%d current_ep=%d\n", src_ep, current_ep);
	assert(src_ep != current_ep);
	
	if (src_ep != ANY) 
		src_nr = _ENDPOINT_P(src_ep);
	else
		src_nr = ANY;
		
	assert( (src_nr >= (-NR_TASKS) && src_nr < NR_PROCS) || (src_nr == ANY) );
	
	/*--------------------------------------*/
	/* NOTIFY PENDING LOOP		*/
	/*--------------------------------------*/
	LIBDEBUG("NOTIFY PENDING LOOP p_pending=%lX\n",current_ptr->p_pending);
	current_ptr->p_msg 	= mptr;
	if( current_ptr->p_pending != 0) {
		for (i = 0; i < sizeof (unsigned long); i++) {
			if ( TEST_BIT(current_ptr->p_pending, i) ) {
				p_nr = i - NR_TASKS;
				if( src_nr != p_nr && src_nr != ANY )
					continue;
				BUILD_NOTIFY_MSG(current_ptr, src_nr);
				LIBDEBUG(MSG9_FORMAT,MSG9_FIELDS(current_ptr->p_msg));
				CLR_BIT(current_ptr->p_pending, i);
				return(OK);	/* report success */
			}
		}
	}
		
	LIBDEBUG("Searching for a message\n");
    xpp = &current_ptr->p_caller_q;
    while (*xpp != NULL) {
		src_ptr = (*xpp);
		LIBDEBUG("src_ep=%d src_ptr->p_endpoint=%d\n", src_ep, src_ptr->p_endpoint);
        if (src_ep == ANY || src_ep == src_ptr->p_endpoint) {
			memcpy(mptr, src_ptr->p_msg, sizeof(message));
			mptr->m_source = src_ptr->p_endpoint;
			src_ptr->p_sendto = NONE;
			src_ptr->p_error = OK;
			CLR_BIT(src_ptr->p_rts_flags, SENDING);
			*xpp = src_ptr->p_q_link;
			LIBDEBUG("p_rts_flags:%lX\n", src_ptr->p_rts_flags );
			if(src_ptr->p_rts_flags == 0)
				taskwakeup(&src_ptr->p_rendez);
            ERROR_RETURN(OK);				/* report success */
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

	LIBDEBUG("Valid message not found\n");
	SET_BIT(current_ptr->p_rts_flags,RECEIVING);
	current_ptr->p_getfrom = src_ep;
	LIBDEBUG("before tasksleep %d\n", current_ptr->p_rendez);
	tasksleep(&current_ptr->p_rendez);
	LIBDEBUG("after tasksleep\n");
	rcode = current_ptr->p_error;
	return(rcode);
}	

