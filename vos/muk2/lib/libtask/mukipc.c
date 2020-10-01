
#include "taskimpl.h"

Task	*timeout_task;
#define MUK_TIMER_INTERVAL 		1000
#define SETTIMEOUT			MOLVOID3


extern Rendez muk_cond;
extern Rendez tout_cond;
extern int	muk_mutex; // pseudo-mutex 

TAILQ_HEAD(tailhead, Task) tout_head;
struct tailhead	*tout_ptr;

void 	muk_enqueue_to(int timeout_ms)
{
	Task *t, *t_ptr;
	long total_to;
	
	t = current_task(); 
	LIBDEBUG("name=%s id=%d timeout_ms:%ld\n", t->name, t->id, timeout_ms);
	t->p_error = OK; 
	
	assert(timeout_ms > 0);
	timeout_ms = (timeout_ms < MUK_TIMER_INTERVAL)?MUK_TIMER_INTERVAL:timeout_ms;

	// void list 
	if( TAILQ_EMPTY(tout_ptr)){
		t->p_timeout = timeout_ms;
		LIBDEBUG("TAILQ_INSERT_HEAD name=%s id=%d timeout:%ld\n", t->name, t->id, t->p_timeout);
		TAILQ_INSERT_HEAD(tout_ptr, t, p_entries);
	} else {
		// lowest timeout_ms 
		t_ptr = TAILQ_FIRST(tout_ptr);
		if( t_ptr->p_timeout >= timeout_ms){
			t_ptr->p_timeout	= t_ptr->p_timeout - timeout_ms;
			t->p_timeout 		= timeout_ms;
			LIBDEBUG("TAILQ_INSERT_HEAD name=%s id=%d timeout_ms:%ld\n", t->name, t->id, timeout_ms);
			TAILQ_INSERT_HEAD(tout_ptr, t, p_entries);
		} else { // search the timeout_ms position 
			total_to= t_ptr->p_timeout;
			while( total_to < timeout_ms){
				LIBDEBUG("name=%s id=%d total_to=%d timeout_ms=%d\n", t->name, t->id, total_to, timeout_ms);
				if (TAILQ_NEXT(t_ptr, p_entries) == NULL) {
					t->p_timeout = timeout_ms - total_to;
					LIBDEBUG("TAILQ_INSERT_TAIL name=%s id=%d timeout_ms:%ld\n", t->name, t->id, timeout_ms);
					TAILQ_INSERT_TAIL(tout_ptr, t, p_entries);
					return;
				}
				t_ptr = TAILQ_NEXT(t_ptr, p_entries);
			}
			t->p_timeout     = timeout_ms-total_to;
			LIBDEBUG("TAILQ_INSERT_BEFORE name=%s id_prev=%d id=%d timeout_ms:%ld\n", t->name, t_ptr->id, t->id, timeout_ms);
			TAILQ_INSERT_BEFORE(t_ptr, t, p_entries);
		}
	}
	return;
}
				
void 	muk_dequeue_to(Task *t)
{
	LIBDEBUG("name=%s id=%d timeout_ms=%d\n", t->name, t->id, t->p_timeout );
	assert(t->p_timeout >= 0);
	t->p_timeout = TIMEOUT_FOREVER;
	t->p_error = OK; 
	TAILQ_REMOVE(tout_ptr, t, p_entries);  /* Deletion. */
}


/*===========================================================================*
 *				muk_tout_task				     *
 * This function is used to manage MUK timeouts 
 *===========================================================================*/
int muk_tout_task(void )
{
	Task *t;

	timeout_task = current_task();
	LIBDEBUG("TIMEOUT TASK ID=%d\n", timeout_task->id);
	taskname("%s","muk_tout_task");
	
	// SETUP the HEAD of the timeout_ms list 
	TAILQ_INIT(&tout_head);
	
	tout_ptr = &tout_head;

  	// SYNCHRONIZE WITH MUK
	MTX_LOCK(muk_mutex);
	COND_SIGNAL(muk_cond);
	COND_WAIT(tout_cond, muk_mutex);
	MTX_UNLOCK(muk_mutex);
		
//	taskdelay(MUK_TIMER_INTERVAL);
	
	while (TRUE) {
		if( TAILQ_EMPTY(tout_ptr)){
			taskdelay(MUK_TIMER_INTERVAL);
			continue;
		}
		t = TAILQ_FIRST(tout_ptr);
		if(t->p_timeout < 0) {
			LIBDEBUG("ASSERT t->p_timeout(%d) <= 0 id=%d name=%s\n",t->p_timeout, t->id, t->name);
			TAILQ_REMOVE(tout_ptr, t, p_entries);
			continue;
		}
		t->p_timeout -= MUK_TIMER_INTERVAL;
		if(	t->p_timeout <= 0) {
			LIBDEBUG("TAILQ_REMOVE name=%s id=%d timeout_ms=%d\n", t->name, t->id, t->p_timeout );
			TAILQ_REMOVE(tout_ptr, t, p_entries);
			t->p_timeout = TIMEOUT_FOREVER;
			t->p_error = EDVSTIMEDOUT; 
			taskwakeup(&t->p_rendez);
		}
	}
}


/*===========================================================================*
 *				muk_send_T				     *
 *===========================================================================*/
int muk_send_T(int dst_ep, message *mptr, long timeout_ms)
{
	muk_proc_t *dst_ptr, *src_ptr;
	muk_proc_t **xpp;
	muk_proc_t *current_ptr;
	proc_usr_t *proc_ptr;
	int rcode, current_nr, current_ep, dst_nr;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_proc->p_nr;
	assert( current_nr != NONE);
 	current_ep = current_ptr->p_proc->p_endpoint;
	assert( get_task(current_ep) != NULL);
	LIBDEBUG("dst_ep:%d current_ep=%d\n", dst_ep, current_ep);
	proc_ptr = current_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	assert(dst_ep != current_ep);
	dst_nr = _ENDPOINT_P(dst_ep);
	assert( dst_nr > (-dc_ptr->dc_nr_tasks) && dst_nr < (dc_ptr->dc_nr_procs));
	dst_ptr = get_task(dst_ep);
	assert( dst_ptr != NULL);
	proc_ptr = dst_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	if( (TEST_BIT(dst_ptr->p_proc->p_rts_flags,BIT_RECEIVING) != 0) 
	 && ((dst_ptr->p_proc->p_getfrom == ANY) || (dst_ptr->p_proc->p_getfrom == current_ep))){
		LIBDEBUG("dst_ep:%d WAITING FOR THIS MESSAGE\n", dst_ep);
		memcpy(dst_ptr->p_msg, mptr, sizeof(message));
		dst_ptr->p_msg->m_source = current_ep;
		dst_ptr->p_proc->p_getfrom = NONE;
		CLR_BIT(dst_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
		dst_ptr->p_error = OK;
		LIBDEBUG("p_proc->p_rts_flags:%lX\n", dst_ptr->p_proc->p_rts_flags );
		if( dst_ptr->p_proc->p_rts_flags == 0){
			if( dst_ptr->p_timeout > 0)
				muk_dequeue_to(dst_ptr);
			taskwakeup(&dst_ptr->p_rendez);
		}		
	} else { // not receiving 
		LIBDEBUG("dst_ep:%d NOT RECEIVING\n", dst_ep);
		if(timeout_ms == TIMEOUT_NOWAIT) 
			ERROR_RETURN(EDVSAGAIN);
		if(timeout_ms != TIMEOUT_FOREVER) 
			muk_enqueue_to(timeout_ms);
		current_ptr->p_proc->p_sendto = dst_ep;
		SET_BIT(current_ptr->p_proc->p_rts_flags, BIT_SENDING);
		current_ptr->p_msg = mptr;
		current_ptr->p_msg->m_source = current_ep;
		/* Process is now blocked.  Put in on the destination's queue. */
		xpp = &dst_ptr->p_caller_q;		/* find end of list */
		while (*xpp != NULL) xpp = &(*xpp)->p_q_link;	
		*xpp = current_ptr;			/* add caller to end */
		current_ptr->p_q_link = NULL;	/* mark new end of list */
		LIBDEBUG("before tasksleep %d\n", current_ptr->p_proc->p_nr);
		tasksleep(&current_ptr->p_rendez);
		LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_proc->p_nr, current_ptr->p_error);
		rcode = current_ptr->p_error;
		if( rcode < 0){
			CLR_BIT(current_ptr->p_proc->p_rts_flags, BIT_SENDING);
			current_ptr->p_proc->p_sendto = NONE;
			xpp = &dst_ptr->p_caller_q;		/* find end of list */
			while (*xpp != NULL) {
				src_ptr = (*xpp);
				LIBDEBUG("current_ep=%d src_ptr->p_proc->p_endpoint=%d\n", current_ep, src_ptr->p_proc->p_endpoint);
				if (src_ptr->p_proc->p_endpoint == current_ep) {
					*xpp = src_ptr->p_q_link;
					break;
				}
				xpp = &(*xpp)->p_q_link;		/* proceed to next */
			}
			if( current_ptr->p_timeout > 0)
				muk_dequeue_to(current_ptr);	
		}
	}
	return(rcode);
}			

/*===========================================================================*
 *				muk_receive_T				     *
 *===========================================================================*/
int muk_receive_T(int src_ep, message *mptr, long timeout_ms)
{
	muk_proc_t *src_ptr;
	muk_proc_t **xpp;
	int i, p_nr, src_nr;
	muk_proc_t *current_ptr;
	proc_usr_t *proc_ptr;
	int rcode, current_nr, current_ep;
	
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_proc->p_nr;
	assert( current_nr != NONE);
 	current_ep = current_ptr->p_proc->p_endpoint;
	assert( get_task(current_ep) != NULL);
	proc_ptr = current_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	LIBDEBUG("src_ep:%d current_ep=%d\n", src_ep, current_ep);
	if (src_ep != ANY) {
		src_nr = _ENDPOINT_P(src_ep);
		assert( src_nr > (-dc_ptr->dc_nr_tasks) && src_nr < (dc_ptr->dc_nr_procs));
		assert(src_ep != current_ep);
		src_ptr = get_task(src_ep);
		assert( src_ptr != NULL);
		proc_ptr = src_ptr->p_proc;
		LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
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
		LIBDEBUG("src_ep=%d src_ptr->p_proc->p_endpoint=%d\n", src_ep, src_ptr->p_proc->p_endpoint);
        if (src_ep == ANY || src_ep == src_ptr->p_proc->p_endpoint) {
			memcpy(mptr, src_ptr->p_msg, sizeof(message));
			mptr->m_source = src_ptr->p_proc->p_endpoint;
			src_ptr->p_proc->p_sendto = NONE;
			src_ptr->p_error = OK;
			CLR_BIT(src_ptr->p_proc->p_rts_flags, BIT_SENDING);
			*xpp = src_ptr->p_q_link;
			LIBDEBUG("p_proc->p_rts_flags:%lX\n", src_ptr->p_proc->p_rts_flags );
			if(src_ptr->p_proc->p_rts_flags == 0){
				if(src_ptr->p_timeout > 0) 
					muk_dequeue_to(src_ptr);
				taskwakeup(&src_ptr->p_rendez);	
			}
            ERROR_RETURN(OK);				/* report success */
		}
		xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

	LIBDEBUG("Valid message was not found\n");
	if(timeout_ms == TIMEOUT_NOWAIT) 
		ERROR_RETURN(EDVSAGAIN);
	if(timeout_ms != TIMEOUT_FOREVER) 
		muk_enqueue_to(timeout_ms);		
	SET_BIT(current_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
	current_ptr->p_proc->p_getfrom = src_ep;
	proc_ptr = current_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
	LIBDEBUG("before tasksleep %d\n", current_ptr->p_proc->p_nr);
	tasksleep(&current_ptr->p_rendez);
	LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_proc->p_nr, current_ptr->p_error);
	rcode = current_ptr->p_error;
	return(rcode);
}	

/*--------------------------------------------------------------*/
/*			muk_notify				*/
/*--------------------------------------------------------------*/
int muk_notify_X(int src_ep, int dst_ep)
{

	muk_proc_t *dst_ptr, *current_ptr, *src_ptr;
	int current_nr, current_ep, src_nr;
	int dst_nr;
	proc_usr_t *proc_ptr;

	LIBDEBUG("src_ep=%d dst_ep=%d\n",src_ep, dst_ep);

	current_ptr = current_task();
	current_nr = current_ptr->p_proc->p_nr;
	assert( current_nr != NONE);
 	current_ep = current_ptr->p_proc->p_endpoint;
	assert( get_task(current_ep) != NULL);
	proc_ptr = current_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	assert(dst_ep != current_ep);
	dst_nr = _ENDPOINT_P(dst_ep);
	assert( dst_nr > (-dc_ptr->dc_nr_tasks) && dst_nr < (dc_ptr->dc_nr_procs));
	dst_ptr =get_task(dst_ep);
	assert( dst_ptr != NULL);
	proc_ptr = dst_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	if( src_ep == SELF){
		src_nr = current_nr;
		src_ep = current_ep;
	}else{
		src_nr = _ENDPOINT_P(src_ep);
		assert( src_nr > (-dc_ptr->dc_nr_tasks) && src_nr < (dc_ptr->dc_nr_procs));
		src_ptr = get_task(src_ep);
		src_nr = src_ptr->p_proc->p_nr;
		proc_ptr = src_ptr->p_proc;
		LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	}
	
	current_ptr->p_error= OK;
	/* Check if 'dst' is blocked waiting for this message.   */
	if (  (TEST_BIT(dst_ptr->p_proc->p_rts_flags, BIT_RECEIVING) 
			&& !TEST_BIT(dst_ptr->p_proc->p_rts_flags, BIT_SENDING) )&&
		(dst_ptr->p_proc->p_getfrom == ANY || dst_ptr->p_proc->p_getfrom == src_ep)) {
		LIBDEBUG("destination is waiting. Build the message and wakeup destination\n");
	
		BUILD_NOTIFY_MSG(dst_ptr, src_nr);
		SET_BIT(dst_ptr->p_proc->p_misc_flags, MIS_BIT_NOTIFY);
		CLR_BIT(dst_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
		dst_ptr->p_proc->p_getfrom 	= NONE;
		dst_ptr->p_error 	= OK;
		if(dst_ptr->p_proc->p_rts_flags == 0){
			if(dst_ptr->p_timeout > 0)
				muk_dequeue_to(dst_ptr);
			taskwakeup(&dst_ptr->p_rendez);
		}
		return(OK);
	} else { 
		LIBDEBUG("destination is not waiting dst_ptr->p_proc->p_rts_flags=%lX\n",dst_ptr->p_proc->p_rts_flags);
		/* Add to destination the bit map with pending notifications  */
		if( TEST_BIT( dst_ptr->p_pending, (src_nr-dc_ptr->dc_nr_tasks))){ // Already pending 
			ERROR_PRINT(EDVSOVERRUN);
		}
		SET_BIT(dst_ptr->p_pending, (src_nr-dc_ptr->dc_nr_tasks)); 
	}
	return(OK);
}

/*--------------------------------------------------------------*/
/*			muk_sendrec_T			*/
/*--------------------------------------------------------------*/
int muk_sendrec_T(int srcdst_ep, message* m_ptr, long timeout_ms)
{
	muk_proc_t *srcdst_ptr, *current_ptr;
	muk_proc_t **xpp, *src_ptr;
	int rcode;
	proc_usr_t *proc_ptr;
	int current_nr, current_id, current_ep;
	int srcdst_nr;
	
	assert(timeout_ms != TIMEOUT_NOWAIT); 
			
	rcode = OK;
	current_ptr = current_task();
	current_nr = current_ptr->p_proc->p_nr;
	assert(current_nr != NONE);
 	current_ep = current_ptr->p_proc->p_endpoint;
	assert( get_task(current_ep) != NULL);
	current_id   = current_ptr->id;
	proc_ptr = current_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	
	LIBDEBUG("srcdst_ep:%d\n", srcdst_ep);
	srcdst_nr = _ENDPOINT_P(srcdst_ep);
	assert( srcdst_nr > (-dc_ptr->dc_nr_tasks) && srcdst_nr < (dc_ptr->dc_nr_procs));
	assert(srcdst_ep != current_ep);
	srcdst_ptr = get_task(srcdst_ep);
	assert( srcdst_ptr != NULL);
	proc_ptr = srcdst_ptr->p_proc;
	LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));

	current_ptr->p_msg	= m_ptr;

	/*--------------------------------------*/
	/* SENDING/RECEIVING		*/
	/*--------------------------------------*/
	LIBDEBUG("SENDING HALF\n");
	current_ptr->p_error	= OK;
	current_ptr->p_proc->p_getfrom  = srcdst_ep;
	if (  (TEST_BIT(srcdst_ptr->p_proc->p_rts_flags, BIT_RECEIVING) 
		&& !TEST_BIT(srcdst_ptr->p_proc->p_rts_flags, BIT_SENDING)) &&
		(srcdst_ptr->p_proc->p_getfrom == ANY || srcdst_ptr->p_proc->p_getfrom == current_ep)) {
		LIBDEBUG("destination is waiting. Copy the message and wakeup destination\n");
		CLR_BIT(srcdst_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
		srcdst_ptr->p_proc->p_getfrom 	= NONE;
		memcpy(srcdst_ptr->p_msg, m_ptr, sizeof(message) );
		srcdst_ptr->p_msg->m_source = current_ep;
		if(srcdst_ptr->p_proc->p_rts_flags == 0) {
			if(srcdst_ptr->p_timeout != TIMEOUT_FOREVER)
				muk_dequeue_to(srcdst_ptr);
			taskwakeup(&srcdst_ptr->p_rendez);
		}
		SET_BIT(current_ptr->p_proc->p_rts_flags, BIT_RECEIVING); /* Sending part: completed, now receiving.. */
		current_ptr->p_proc->p_getfrom = srcdst_ep;
		proc_ptr = current_ptr->p_proc;
		if(timeout_ms != TIMEOUT_FOREVER) 
			muk_enqueue_to(timeout_ms);
		LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
		LIBDEBUG("before tasksleep %d\n", current_ptr->p_proc->p_nr);
		tasksleep(&current_ptr->p_rendez);
		LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_proc->p_nr, current_ptr->p_error);
		rcode = current_ptr->p_error;
		if( rcode < 0) {
			CLR_BIT(current_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
			CLR_BIT(current_ptr->p_proc->p_rts_flags, BIT_SENDING);
			current_ptr->p_proc->p_getfrom  = NONE;
			current_ptr->p_proc->p_sendto 	= NONE;
		}
	} else { 
		LIBDEBUG("destination is not waiting srcdst_ptr-flags=%lX\n"
			,srcdst_ptr->p_proc->p_rts_flags);
		if(timeout_ms == TIMEOUT_NOWAIT) ERROR_RETURN(EDVSAGAIN);
		LIBDEBUG("Enqueue at TAIL.\n");
		/* The destination is not waiting for this message 			*/
		/* Append the caller at the TAIL of the destination senders' queue	*/
		/* blocked sending the message */
				
		current_ptr->p_msg->m_source = current_ptr->p_proc->p_endpoint;
		SET_BIT(current_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
		SET_BIT(current_ptr->p_proc->p_rts_flags, BIT_SENDING);
		current_ptr->p_proc->p_sendto 	= srcdst_ep;
		
		/* Process is now blocked.  Put in on the destination's queue. */
		xpp = &srcdst_ptr->p_caller_q;		/* find end of list */
		while (*xpp != NULL) xpp = &(*xpp)->p_q_link;	
		*xpp = current_ptr;			/* add caller to end */
		current_ptr->p_q_link = NULL;	/* mark new end of list */
		if(timeout_ms != TIMEOUT_FOREVER) 
			muk_enqueue_to(timeout_ms);
		LIBDEBUG("before tasksleep %d\n", current_ptr->p_proc->p_nr);
		tasksleep(&current_ptr->p_rendez);
		LIBDEBUG("after tasksleep %d, p_error=%d \n", current_ptr->p_proc->p_nr, current_ptr->p_error);
		rcode = current_ptr->p_error;
		if( rcode < 0) {
			LIBDEBUG("removing %d link from %d list.\n", current_ptr->p_proc->p_endpoint, srcdst_ep);
			CLR_BIT(current_ptr->p_proc->p_rts_flags, BIT_RECEIVING);
			CLR_BIT(current_ptr->p_proc->p_rts_flags, BIT_SENDING);
			current_ptr->p_proc->p_getfrom  = NONE;
			current_ptr->p_proc->p_sendto 	= NONE;
			xpp = &srcdst_ptr->p_caller_q;		/* find end of list */
			while (*xpp != NULL) {
				src_ptr = (*xpp);
				LIBDEBUG("current_ep=%d src_ptr->p_proc->p_endpoint=%d\n", current_ep, src_ptr->p_proc->p_endpoint);
				if (src_ptr->p_proc->p_endpoint == current_ep) {
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

/*--------------------------------------------------------------*/
/*			muk_wait4bind_X									*/
/*--------------------------------------------------------------*/
#define	TIMEOUT_1SEC	1000

int muk_wait4bind_X(int oper, int other_ep, long timeout_ms)
{
	muk_proc_t *current_ptr, *other_ptr;
	int ret;
	int current_id, current_ep, other_nr, current_nr;
	proc_usr_t *proc_ptr;
	
	current_ptr = current_task();
	current_id = current_ptr->id;
	
	LIBDEBUG("id=%d oper=%d other_ep=%d timeout_ms=%ld\n", 
		oper, other_ep, timeout_ms);

	assert( oper == WAIT_BIND  || oper == WAIT_UNBIND);
	if ( other_ep == SELF) { 
		current_nr = current_ptr->p_proc->p_nr;
		assert( current_nr != NONE);
		current_ep = current_ptr->p_proc->p_endpoint;
		assert( get_task(current_ep) != NULL);
		proc_ptr = current_ptr->p_proc;
		other_ep = current_ep;
		LIBDEBUG(PROC_USR_FORMAT,PROC_USR_FIELDS(proc_ptr));
	}
	
	other_nr = _ENDPOINT_P(other_ep);
	assert( other_nr > (-dc_ptr->dc_nr_tasks) && other_nr < (dc_ptr->dc_nr_procs));

	if( oper == WAIT_BIND ){ 
		if (other_ep == current_ep) {
			do {
				other_ptr = get_task(current_ep); 
				if(other_ptr != NULL) {
					ret = current_ptr->p_proc->p_endpoint;
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
					ret = other_ptr->p_proc->p_endpoint;
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
						ret = current_ptr->p_proc->p_endpoint;
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
						ret = other_ptr->p_proc->p_endpoint;
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