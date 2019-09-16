
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
	current_ptr = taskrunning;
	current_nr = current_ptr->p_nr;

	MUKDEBUG("dst_nr:%d current_nr=%d\n", dst_nr, current_nr);
	assert(dst_nr != current_nr);
	assert(dst_nr >= (-NR_TASKS) && dst_nr < NR_PROCS);
	current_ptr = taskrunning;
	dst_ptr =get_task(dst_nr);
	if( (TEST_BIT(dst_ptr->p_rts_flags,RECEIVING) != 0) 
	 && ((dst_ptr->p_getfrom == ANY) || (dst_ptr->p_getfrom == current_nr))){
		MUKDEBUG("dst_nr:%d WAITING FOR THIS MESSAGE\n", dst_nr);
		memcpy(dst_ptr->p_msg, mptr, sizeof(message));
		dst_ptr->p_msg->m_source = current_nr;
		dst_ptr->p_getfrom = NONE;
		CLR_BIT(dst_ptr->p_rts_flags,RECEIVING);
		dst_ptr->p_error = OK;
		MUKDEBUG("p_rts_flags:%lX\n", dst_ptr->p_rts_flags );
		if( dst_ptr->p_rts_flags == 0){
			taskwakeup(&dst_ptr->p_rendez);
		}		
	} else { // not receiving 
		MUKDEBUG("dst_nr:%d NOT RECEIVENG\n", dst_nr);
		current_ptr->p_sendto = dst_nr;
		SET_BIT(current_ptr->p_rts_flags,SENDING);
		current_ptr->p_msg = mptr;
		current_ptr->p_msg->m_source = current_nr;
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
int muk_receive(int src_nr, message *mptr)
{
	muk_proc_t *src_ptr, *dst_ptr;
	muk_proc_t **xpp;
	int rcode, i, p_nr;
	
	rcode = OK;
	current_ptr = taskrunning;
	current_nr = current_ptr->p_nr;
	
	MUKDEBUG("src_nr:%d current_nr=%d\n", src_nr, current_nr);
	assert(src_nr != current_nr);
	assert( (src_nr >= (-NR_TASKS) && src_nr < NR_PROCS) || (src_nr == ANY) );
	
	/*--------------------------------------*/
	/* NOTIFY PENDING LOOP		*/
	/*--------------------------------------*/
	MUKDEBUG("NOTIFY PENDING LOOP p_pending=%lX\n",current_ptr->p_pending);
	current_ptr->p_msg 	= mptr;
	if( current_ptr->p_pending != 0) {
		for (i = 0; i < sizeof (unsigned long); i++) {
			if ( TEST_BIT(current_ptr->p_pending, i) ) {
				p_nr = i - NR_TASKS;
				if( src_nr != p_nr && src_nr != ANY )
					continue;
				BUILD_NOTIFY_MSG(current_ptr, src_nr);
				MUKDEBUG(MSG9_FORMAT,MSG9_FIELDS(current_ptr->p_msg));
				CLR_BIT(current_ptr->p_pending, i);
				return(OK);	/* report success */
			}
		}
	}
		
	MUKDEBUG("Searching for a message\n");
    xpp = &current_ptr->p_caller_q;
    while (*xpp != NULL) {
		src_ptr = (*xpp);
		MUKDEBUG("src_nr=%d src_ptr->p_nr=%d\n", src_nr, src_ptr->p_nr);
        if (src_nr == ANY || src_nr == src_ptr->p_nr) {
			memcpy(mptr, src_ptr->p_msg, sizeof(message));
			mptr->m_source = src_ptr->p_nr;
			src_ptr->p_sendto = NONE;
			src_ptr->p_error = OK;
			CLR_BIT(src_ptr->p_rts_flags, SENDING);
			*xpp = src_ptr->p_q_link;
			MUKDEBUG("p_rts_flags:%lX\n", src_ptr->p_rts_flags );
			if(src_ptr->p_rts_flags == 0)
				taskwakeup(&src_ptr->p_rendez);
            ERROR_RETURN(OK);				/* report success */
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

	MUKDEBUG("Valid message not found\n");
	SET_BIT(current_ptr->p_rts_flags,RECEIVING);
	current_ptr->p_getfrom = src_nr;
	MUKDEBUG("before tasksleep %d\n", current_ptr->p_rendez);
	tasksleep(&current_ptr->p_rendez);
	MUKDEBUG("after tasksleep\n");
	rcode = current_ptr->p_error;
	return(rcode);
}	

