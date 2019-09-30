
#include "taskimpl.h"

Task	*timeout_task;
#define MUK_TIMER_INTERVAL 		1000

void 	muk_set_timeout(int timeout)
{
	Task *t, *t_ptr;
	long next_to, total_to;
	
	t = current_task(); 
	LIBDEBUG("id=%d timeout:%ld\n", t->id, timeout);
	
	assert(t->p_next_timeout == TIMEOUT_FOREVER);
	assert(t->p_t_link == NULL);
	assert(timeout > 0);
	timeout = (timeout < MUK_TIMER_INTERVAL)?MUK_TIMER_INTERVAL:timeout;
	
	// void list 
	if( timeout_task->p_t_link == NULL){
		t->p_t_link = NULL;
		t->p_next_timeout = 0;
		timeout_task->p_next_timeout = timeout;
		timeout_task->p_t_link = t;
	} else {
		// lowest timeout 
		if( timeout_task->p_next_timeout >= timeout){
			t->p_next_timeout 	= timeout_task->p_next_timeout - timeout;
			t->p_t_link 	= timeout_task->p_t_link;
			timeout_task->p_t_link 		= t;
			timeout_task->p_next_timeout 	= timeout;
		} else { // search the timeout position 
			next_to = timeout_task->p_next_timeout;
			total_to = next_to;
			t_ptr = timeout_task->p_t_link;
			while(total_to < timeout){
				next_to = t_ptr->p_next_timeout;
				if( next_to != TIMEOUT_FOREVER){
					if( (total_to + next_to) > timeout){
						t = t_ptr->p_t_link;
						t_ptr->p_t_link = t;
						t_ptr->p_next_timeout = timeout-total_to;
						t->p_next_timeout = (total_to + next_to) - timeout;
						break;
					} else{
						total_to += next_to;
						t = t_ptr->p_t_link;
						next_to = t_ptr->p_next_timeout;
					}
				}else{ // enqueue at the tail 
					t = t_ptr->p_t_link;
					t_ptr->p_t_link = t;
					t_ptr->p_next_timeout = timeout-total_to;
					t->p_next_timeout = (total_to + next_to) - timeout;
				}
			}
		}
	}
}
	
/*===========================================================================*
 *				muk_timeout_task				     *
 * This function is used to manage IPC timeouts 
 *===========================================================================*/
int muk_timeout_task(void )
{
	Task *task;

	timeout_task = current_task();
	LIBDEBUG("TIMEOUT TASK ID=%d\n", timeout_task->id);
	
	// SETUP the HEAD of the timeout list 
	timeout_task->p_next_timeout = TIMEOUT_FOREVER;
	timeout_task->p_t_link 	= NULL;

	while(TRUE){
		if( timeout_task->p_next_timeout != 0) {
			taskdelay(MUK_TIMER_INTERVAL);
			if( timeout_task->p_next_timeout != TIMEOUT_FOREVER){
				timeout_task->p_next_timeout -= MUK_TIMER_INTERVAL;
				timeout_task->p_next_timeout = (timeout_task->p_next_timeout < MUK_TIMER_INTERVAL)
												?MUK_TIMER_INTERVAL
												:timeout_task->p_next_timeout;
			}
			continue;
		}
		do {
			assert( timeout_task->p_t_link != NULL);
			task = timeout_task->p_t_link;
			taskwakeup(&task->p_rendez);
			task->p_error = EDVSTIMEDOUT;
			timeout_task->p_next_timeout = task->p_next_timeout;
			timeout_task->p_t_link = task->p_t_link;
			task->p_next_timeout = TIMEOUT_FOREVER;
			task->p_t_link = NULL;
		} while(timeout_task->p_next_timeout == 0);
		if( timeout_task->p_t_link == NULL)
			timeout_task->p_next_timeout = TIMEOUT_FOREVER;
	}	
}


/*===========================================================================*
 *				muk_send_T				     *
 *===========================================================================*/
int muk_send_T(int dst_ep, message *mptr, long timeout)
{
	muk_proc_t *dst_ptr, *src_ptr;
	muk_proc_t **xpp;
	muk_proc_t *current_ptr;
	int rcode, current_nr, current_ep, dst_nr, src_ep;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_nr;
	assert( current_nr != NONE);
 	current_ep = current_ptr->p_endpoint;
	assert( get_task(current_ep) != NULL);
	LIBDEBUG("dst_ep:%d current_ep=%d\n", dst_ep, current_ep);
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(current_ptr));

	assert(dst_ep != current_ep);
	dst_nr = _ENDPOINT_P(dst_ep);
	assert( dst_nr > (-dc_ptr->dc_nr_tasks) && dst_nr < (dc_ptr->dc_nr_procs));
	dst_ptr = get_task(dst_ep);
	assert( dst_ptr != NULL);
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(dst_ptr));

	if( (TEST_BIT(dst_ptr->p_rts_flags,BIT_RECEIVING) != 0) 
	 && ((dst_ptr->p_getfrom == ANY) || (dst_ptr->p_getfrom == current_ep))){
		LIBDEBUG("dst_ep:%d WAITING FOR THIS MESSAGE\n", dst_ep);
		memcpy(dst_ptr->p_msg, mptr, sizeof(message));
		dst_ptr->p_msg->m_source = current_ep;
		dst_ptr->p_getfrom = NONE;
		CLR_BIT(dst_ptr->p_rts_flags, BIT_RECEIVING);
		dst_ptr->p_error = OK;
		LIBDEBUG("p_rts_flags:%lX\n", dst_ptr->p_rts_flags );
		if( dst_ptr->p_rts_flags == 0){
			taskwakeup(&dst_ptr->p_rendez);
		}		
	} else { // not receiving 
		LIBDEBUG("dst_ep:%d NOT RECEIVING\n", dst_ep);
		if(timeout == TIMEOUT_NOWAIT) ERROR_RETURN(EDVSAGAIN);
		current_ptr->p_sendto = dst_ep;
		SET_BIT(current_ptr->p_rts_flags, BIT_SENDING);
		current_ptr->p_msg = mptr;
		current_ptr->p_msg->m_source = current_ep;
		/* Process is now blocked.  Put in on the destination's queue. */
		xpp = &dst_ptr->p_caller_q;		/* find end of list */
		while (*xpp != NULL) xpp = &(*xpp)->p_q_link;	
		*xpp = current_ptr;			/* add caller to end */
		current_ptr->p_q_link = NULL;	/* mark new end of list */
		LIBDEBUG("before tasksleep %d\n", current_ptr->p_nr);
		tasksleep(&current_ptr->p_rendez);
		LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_nr, current_ptr->p_error);
		rcode = current_ptr->p_error;
		if( rcode < 0){
			CLR_BIT(current_ptr->p_rts_flags, BIT_SENDING);
			current_ptr->p_sendto = NONE;
			xpp = &dst_ptr->p_caller_q;		/* find end of list */
			while (*xpp != NULL) {
				src_ptr = (*xpp);
				LIBDEBUG("current_ep=%d src_ptr->p_endpoint=%d\n", current_ep, src_ptr->p_endpoint);
				if (src_ptr->p_endpoint == current_ep) {
					*xpp = src_ptr->p_q_link;
					break;
				}
				xpp = &(*xpp)->p_q_link;		/* proceed to next */
			}
		}
	}
	return(rcode);
}			

/*===========================================================================*
 *				muk_receive_T				     *
 *===========================================================================*/
int muk_receive_T(int src_ep, message *mptr, long timeout)
{
	muk_proc_t *src_ptr, *dst_ptr;
	muk_proc_t **xpp;
	int i, p_nr, src_nr;
	muk_proc_t *current_ptr;
	int rcode, current_nr, current_ep;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_nr;
	assert( current_nr != NONE);
 	current_ep = current_ptr->p_endpoint;
	assert( get_task(current_ep) != NULL);
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(current_ptr));

	LIBDEBUG("src_ep:%d current_ep=%d\n", src_ep, current_ep);
	if (src_ep != ANY) {
		src_nr = _ENDPOINT_P(src_ep);
		assert( src_nr > (-dc_ptr->dc_nr_tasks) && src_nr < (dc_ptr->dc_nr_procs));
		assert(src_ep != current_ep);
		src_ptr = get_task(src_ep);
		assert( src_ptr != NULL);
		LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(src_ptr));
	} else{
		src_nr = ANY;
	}
		
	/*--------------------------------------*/
	/* NOTIFY PENDING LOOP		*/
	/*--------------------------------------*/
	LIBDEBUG("NOTIFY PENDING LOOP p_pending=%lX\n",current_ptr->p_pending);
	current_ptr->p_msg 	= mptr;
	if( current_ptr->p_pending != 0) {
		for (i = 0; i < sizeof (unsigned long); i++) {
			if ( TEST_BIT(current_ptr->p_pending, i) ) {
				p_nr = i - dc_ptr->dc_nr_tasks;
				if( src_nr != p_nr && src_nr != ANY )
					continue;
				BUILD_NOTIFY_MSG(current_ptr, src_nr);
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
			CLR_BIT(src_ptr->p_rts_flags, BIT_SENDING);
			*xpp = src_ptr->p_q_link;
			LIBDEBUG("p_rts_flags:%lX\n", src_ptr->p_rts_flags );
			if(src_ptr->p_rts_flags == 0)
				taskwakeup(&src_ptr->p_rendez);
            ERROR_RETURN(OK);				/* report success */
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

	LIBDEBUG("Valid message was not found\n");
	if(timeout == TIMEOUT_NOWAIT) ERROR_RETURN(EDVSAGAIN);

	SET_BIT(current_ptr->p_rts_flags, BIT_RECEIVING);
	current_ptr->p_getfrom = src_ep;
	LIBDEBUG(PROC_MUK_FORMAT, PROC_MUK_FIELDS(current_ptr));
	LIBDEBUG("before tasksleep %d\n", current_ptr->p_nr);
	tasksleep(&current_ptr->p_rendez);
	LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_nr, current_ptr->p_error);
	rcode = current_ptr->p_error;
	return(rcode);
}	

/*--------------------------------------------------------------*/
/*			muk_notify				*/
/*--------------------------------------------------------------*/
int muk_notify_X(int src_ep, int dst_ep)
{

	muk_proc_t *dst_ptr, *current_ptr, *sproxy_ptr, *src_ptr;
	int current_nr, current_id, current_ep, src_nr;
	int dst_nr;
	struct task_struct *task_ptr;	
	struct timespec *t_ptr;
	message *mptr;

	LIBDEBUG("src_ep=%d dst_ep=%d\n",src_ep, dst_ep);

	current_ptr = current_task();
	current_nr = current_ptr->p_nr;
	assert( current_nr != NONE);
 	current_ep = current_ptr->p_endpoint;
	assert( get_task(current_ep) != NULL);
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(current_ptr));

	assert(dst_ep != current_ep);
	dst_nr = _ENDPOINT_P(dst_ep);
	assert( dst_nr > (-dc_ptr->dc_nr_tasks) && dst_nr < (dc_ptr->dc_nr_procs));
	dst_ptr =get_task(dst_ep);
	assert( dst_ptr != NULL);
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(dst_ptr));

	if( src_nr == SELF){
		src_nr = current_nr;
		src_ep = current_ep;
	}else{
		src_nr = _ENDPOINT_P(src_ep);
		assert( src_nr > (-dc_ptr->dc_nr_tasks) && src_nr < (dc_ptr->dc_nr_procs));
		src_ptr = get_task(src_ep);
		src_nr = src_ptr->p_nr;
		LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(src_ptr));
	}
	
	current_ptr->p_error= OK;
	/* Check if 'dst' is blocked waiting for this message.   */
	if (  (TEST_BIT(dst_ptr->p_rts_flags, BIT_RECEIVING) 
			&& !TEST_BIT(dst_ptr->p_rts_flags, BIT_SENDING) )&&
		(dst_ptr->p_getfrom == ANY || dst_ptr->p_getfrom == src_ep)) {
		LIBDEBUG("destination is waiting. Build the message and wakeup destination\n");
	
		BUILD_NOTIFY_MSG(dst_ptr, src_nr);
		SET_BIT(dst_ptr->p_misc_flags, MIS_BIT_NOTIFY);
		CLR_BIT(dst_ptr->p_rts_flags, BIT_RECEIVING);
		dst_ptr->p_getfrom 	= NONE;
		dst_ptr->p_error 	= OK;
		if(dst_ptr->p_rts_flags == 0)
			taskwakeup(&dst_ptr->p_rendez);
		return(OK);
	} else { 
		LIBDEBUG("destination is not waiting dst_ptr->p_rts_flags=%lX\n",dst_ptr->p_rts_flags);
		/* Add to destination the bit map with pending notifications  */
		if( TEST_BIT( dst_ptr->p_pending, (src_nr-dc_ptr->dc_nr_tasks))){ // Already pending 
			ERROR_PRINT(EDVSOVERRUN);
		}
		SET_BIT(dst_ptr->p_pending, (src_nr-dc_ptr->dc_nr_tasks)); 
	}
	return(OK);
}

/*--------------------------------------------------------------*/
/*			muk_wait4bind_X									*/
/*--------------------------------------------------------------*/
#define	TIMEOUT_1SEC	1000

int muk_wait4bind_X(int oper, int other_ep, long timeout_ms)
{
	muk_proc_t *current_ptr, *other_ptr;
	muk_proc_t *uproc_ptr;
	int ret;
	int current_id, current_nr, current_ep, other_nr;
	
	LIBDEBUG("oper=%d other_ep=%d timeout_ms=%ld\n", 
		oper, other_ep, timeout_ms);

	assert( oper == WAIT_BIND  || oper == WAIT_UNBIND);
	current_nr = _ENDPOINT_P(current_ep);
	if ( other_ep == SELF) other_ep = current_ep;
	
	other_nr = _ENDPOINT_P(other_ep);
	assert( other_nr > (-dc_ptr->dc_nr_tasks) && other_nr < (dc_ptr->dc_nr_procs));

	LIBDEBUG("current_id=%d ret=%d\n", current_id, ret);
	if( oper == WAIT_BIND ){ 
		if (other_ep == current_ep) {
			do {
				other_ptr = get_task(current_ep); 
				if(other_ptr != NULL) {
					ret = current_ptr->p_endpoint;
					break;
				} else {
					if( timeout_ms == TIMEOUT_NOWAIT){ 
						ret = EDVSNOTBIND;
						break;
					} else {
						if( timeout_ms != TIMEOUT_FOREVER){
							taskdelay(TIMEOUT_1SEC);
							timeout_ms -= TIMEOUT_1SEC;
							if( timeout_ms < 0)
								timeout_ms = TIMEOUT_NOWAIT;
						}else{
							taskdelay(TIMEOUT_1SEC);
						}
					}
				}
			} while(other_ptr == NULL);
			if( ret < EDVSERRCODE)
				ERROR_RETURN(ret);
			LIBDEBUG("ret=%d\n", ret);
			return(ret);
		} else{ 
			/*------------------------------------------
			* get the OTHER process number
			*------------------------------------------*/
			other_nr = _ENDPOINT_P(other_ep);			
			do {
				other_ptr = get_task(other_ep); 
				if(other_ptr != NULL) {
					ret = other_ptr->p_endpoint;
					break;
				} else {
					if( timeout_ms == TIMEOUT_NOWAIT){ 
						ret = EDVSNOTBIND;
						break;
					} else {
						if( timeout_ms != TIMEOUT_FOREVER){
							taskdelay(TIMEOUT_1SEC);
							timeout_ms -= TIMEOUT_1SEC;
							if( timeout_ms < 0)
								timeout_ms = TIMEOUT_NOWAIT;
						}else{
							taskdelay(TIMEOUT_1SEC);
						}
					}
				}
			} while(other_ptr == NULL);
			if( ret < EDVSERRCODE)
				ERROR_RETURN(ret);
			LIBDEBUG("ret=%d\n", ret);
			return(ret);
		} 
	}else{ // WAIT4UNBIND //////////////////////////////
		if (other_ep == current_ep) {
			do {
				other_ptr = get_task(current_ep); 
				if(other_ptr == NULL) {
					ret = EDVSNOTBIND;
					break;
				} else {
					if( timeout_ms == TIMEOUT_NOWAIT){ 
						ret = current_ptr->p_endpoint;
						break;
					} else {
						if( timeout_ms != TIMEOUT_FOREVER){
							taskdelay(timeout_ms);
							timeout_ms = TIMEOUT_NOWAIT;
						}else{
							taskdelay(TIMEOUT_MS);
						}
					}
				}
			} while(other_ptr != NULL);
			if( ret < EDVSERRCODE)
				ERROR_RETURN(ret);
			LIBDEBUG("ret=%d\n", ret);
			return(ret);
		} else{ 
			/*------------------------------------------
			* get the OTHER process number
			*------------------------------------------*/
			other_nr = _ENDPOINT_P(other_ep);			
			do {
				other_ptr = get_task(other_ep); 
				if(other_ptr == NULL) {
					ret = EDVSNOTBIND;
					break;
				} else {
					if( timeout_ms == TIMEOUT_NOWAIT){ 
						ret = other_ptr->p_endpoint;
						break;
					} else {
						if( timeout_ms != TIMEOUT_FOREVER){
							taskdelay(timeout_ms);
							timeout_ms = TIMEOUT_NOWAIT;
						}else{
							taskdelay(TIMEOUT_MS);
						}
					}
				}
			} while(other_ptr != NULL);
			if( ret > EDVSERRCODE)
				ERROR_RETURN(ret);
			LIBDEBUG("ret=%d\n", ret);
			return(ret);
		}	
	}
	if(ret < EDVSERRCODE) ERROR_RETURN(ret);
	return(ret);		
}


/*--------------------------------------------------------------*/
/*			muk_sendrec_T			*/
/*--------------------------------------------------------------*/
int muk_sendrec_T(int srcdst_ep, message* m_ptr, long timeout_ms)
{
	muk_proc_t *srcdst_ptr, *current_ptr;
	muk_proc_t **xpp, *src_ptr;
	int rcode;
	int current_nr, current_id, current_ep;
	int srcdst_nr;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_nr;
	assert(current_nr != NONE);
 	current_ep = current_ptr->p_endpoint;
	assert( get_task(current_ep) != NULL);
	current_id   = current_ptr->id;
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(current_ptr));
	
	LIBDEBUG("srcdst_ep:%d\n", srcdst_ep);
	srcdst_nr = _ENDPOINT_P(srcdst_ep);
	assert( srcdst_nr > (-dc_ptr->dc_nr_tasks) && srcdst_nr < (dc_ptr->dc_nr_procs));
	assert(srcdst_ep != current_ep);
	srcdst_ptr = get_task(srcdst_ep);
	assert( srcdst_ptr != NULL);
	LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(srcdst_ptr));

	current_ptr->p_msg	= m_ptr;

	/*--------------------------------------*/
	/* SENDING/RECEIVING		*/
	/*--------------------------------------*/
	LIBDEBUG("SENDING HALF\n");
	current_ptr->p_error	= OK;
	current_ptr->p_getfrom  = srcdst_ep;
	if (  (TEST_BIT(srcdst_ptr->p_rts_flags, BIT_RECEIVING) 
		&& !TEST_BIT(srcdst_ptr->p_rts_flags, BIT_SENDING)) &&
		(srcdst_ptr->p_getfrom == ANY || srcdst_ptr->p_getfrom == current_ep)) {
		LIBDEBUG("destination is waiting. Copy the message and wakeup destination\n");
		CLR_BIT(srcdst_ptr->p_rts_flags, BIT_RECEIVING);
		srcdst_ptr->p_getfrom 	= NONE;
		memcpy(srcdst_ptr->p_msg, m_ptr, sizeof(message) );
		srcdst_ptr->p_msg->m_source = current_ep;
		if(srcdst_ptr->p_rts_flags == 0) 
			taskwakeup(&srcdst_ptr->p_rendez);
		SET_BIT(current_ptr->p_rts_flags, BIT_RECEIVING); /* Sending part: completed, now receiving.. */
		current_ptr->p_getfrom = srcdst_ep;
		LIBDEBUG(PROC_MUK_FORMAT,PROC_MUK_FIELDS(current_ptr));
		LIBDEBUG("before tasksleep %d\n", current_ptr->p_nr);
		tasksleep(&current_ptr->p_rendez);
		LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_nr, current_ptr->p_error);
		rcode = current_ptr->p_error;
		if( rcode < 0) {
			CLR_BIT(current_ptr->p_rts_flags, BIT_RECEIVING);
			CLR_BIT(current_ptr->p_rts_flags, BIT_SENDING);
			current_ptr->p_getfrom  = NONE;
			current_ptr->p_sendto 	= NONE;
		}
	} else { 
		LIBDEBUG("destination is not waiting srcdst_ptr-flags=%lX\n"
			,srcdst_ptr->p_rts_flags);
		if(timeout_ms == TIMEOUT_NOWAIT) ERROR_RETURN(EDVSAGAIN);
		LIBDEBUG("Enqueue at TAIL.\n");
		/* The destination is not waiting for this message 			*/
		/* Append the caller at the TAIL of the destination senders' queue	*/
		/* blocked sending the message */
				
		current_ptr->p_msg->m_source = current_ptr->p_endpoint;
		SET_BIT(current_ptr->p_rts_flags, BIT_RECEIVING);
		SET_BIT(current_ptr->p_rts_flags, BIT_SENDING);
		current_ptr->p_sendto 	= srcdst_ep;
		
		/* Process is now blocked.  Put in on the destination's queue. */
		xpp = &srcdst_ptr->p_caller_q;		/* find end of list */
		while (*xpp != NULL) xpp = &(*xpp)->p_q_link;	
		*xpp = current_ptr;			/* add caller to end */
		current_ptr->p_q_link = NULL;	/* mark new end of list */
		LIBDEBUG("before tasksleep %d\n", current_ptr->p_nr);
		tasksleep(&current_ptr->p_rendez);
		LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_nr, current_ptr->p_error);
		rcode = current_ptr->p_error;
		if( rcode < 0) {
			LIBDEBUG("removing %d link from %d list.\n", current_ptr->p_endpoint, srcdst_ep);
			CLR_BIT(current_ptr->p_rts_flags, BIT_RECEIVING);
			CLR_BIT(current_ptr->p_rts_flags, BIT_SENDING);
			current_ptr->p_getfrom  = NONE;
			current_ptr->p_sendto 	= NONE;
			xpp = &srcdst_ptr->p_caller_q;		/* find end of list */
			while (*xpp != NULL) {
				src_ptr = (*xpp);
				LIBDEBUG("current_ep=%d src_ptr->p_endpoint=%d\n", current_ep, src_ptr->p_endpoint);
				if (src_ptr->p_endpoint == current_ep) {
					*xpp = src_ptr->p_q_link;
					break;
				}
				xpp = &(*xpp)->p_q_link;		/* proceed to next */
			}
		} 
	}

	if(rcode < 0) ERROR_RETURN(rcode);
	return(rcode);
}


