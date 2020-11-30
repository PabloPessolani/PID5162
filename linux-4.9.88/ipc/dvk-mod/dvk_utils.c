/****************************************************************/
/*			MOL UTILITIES				*/
/****************************************************************/

#include "dvk_mod.h"

/*----------------------------------------------------------------*/
/*			init_proc_desc				*/
/* !!! On input and output the DC mutex must be locked !!!	*/
/*----------------------------------------------------------------*/
void init_proc_desc(struct proc *proc_ptr, int dcid, int index)
{
	int j;
	dc_desc_t *dc_ptr;

	DVKDEBUG(INTERNAL,"p_name=%s dcid=%d\n",proc_ptr->p_usr.p_name, dcid);

	/*Suppose that the dcid has been checked before */
	if( dcid == PROXY_NO_DC) {
		proc_ptr->p_usr.p_nr 	= index;		/* proxies struct index */
	}else{
		dc_ptr = &dc[dcid];
		proc_ptr->p_usr.p_nr 	= (index-(dc_ptr->dc_usr.dc_nr_tasks));
		CLR_SYS_MAP(proc_ptr->p_priv.priv_notify_pending);
	}

	DVKDEBUG(INTERNAL,"Clearing Privileges\n");
	proc_ptr->p_priv.priv_usr.priv_id	   		= 0;
	proc_ptr->p_priv.priv_usr.priv_warn	   		= NONE;
	proc_ptr->p_priv.priv_usr.priv_level     	= USER_PRIV;
	proc_ptr->p_priv.priv_usr.priv_trap_mask 	= 0;
	CLR_SYS_MAP(proc_ptr->p_priv.priv_usr.priv_ipc_to);
	CLR_DVK_MAP(proc_ptr->p_priv.priv_usr.priv_dvk_allowed);
	
	DVKDEBUG(INTERNAL,"Setting Default DVK calls privileges\n");
	set_sys_bit(proc_ptr->p_priv.priv_usr.priv_dvk_allowed,DVK_SENDREC); 
	set_sys_bit(proc_ptr->p_priv.priv_usr.priv_dvk_allowed,DVK_UNBIND); 
	set_sys_bit(proc_ptr->p_priv.priv_usr.priv_dvk_allowed,DVK_GETEP); 
	set_sys_bit(proc_ptr->p_priv.priv_usr.priv_dvk_allowed,DVK_WAIT4BIND); 

	DVKDEBUG(INTERNAL,"Clearing Process fields\n");
	proc_ptr->p_usr.p_dcid 				= dcid;
	proc_ptr->p_usr.p_lpid 				= PROC_NO_PID;
	proc_ptr->p_usr.p_vpid 				= PROC_NO_PID;
	proc_ptr->p_usr.p_rts_flags 		= SLOT_FREE;
	proc_ptr->p_usr.p_misc_flags 		= GENERIC_PROC;
	proc_ptr->p_usr.p_nodeid 			= (-1);			/* OLD: atomic_read(&local_nodeid); */
	proc_ptr->p_usr.p_nodemap 			= 0;
	proc_ptr->p_rcode 					= OK;	

	memset(&proc_ptr->p_message,0,sizeof(message));
	proc_ptr->p_umsg					= NULL;

	proc_ptr->p_usr.p_getfrom			= NONE;
	proc_ptr->p_usr.p_sendto			= NONE;
	proc_ptr->p_usr.p_waitmigr			= NONE;
	proc_ptr->p_usr.p_waitunbind		= NONE;
	proc_ptr->p_usr.p_proxy				= NONE;

	/* Sets the process CPU affinity */
	cpumask_setall(&proc_ptr->p_usr.p_cpumask);
	
	init_waitqueue_head(&proc_ptr->p_wqhead);		/* Initialize the wait queue 		*/
	proc_ptr->p_pseudosem 				= 0;		/* pseudo semaphore			*/
	
	INIT_LIST_HEAD(&proc_ptr->p_list);
	INIT_LIST_HEAD(&proc_ptr->p_link);

	INIT_LIST_HEAD(&proc_ptr->p_mlist);
	INIT_LIST_HEAD(&proc_ptr->p_mlink);

	INIT_LIST_HEAD(&proc_ptr->p_ulist);
	INIT_LIST_HEAD(&proc_ptr->p_ulink);
	
	proc_ptr->p_rmtcmd.c_cmd	= CMD_NONE;
	proc_ptr->p_rmtcmd.c_dcid	= dcid;
	proc_ptr->p_rmtcmd.c_src 	= NONE;	
	proc_ptr->p_rmtcmd.c_dst 	= NONE;
	proc_ptr->p_rmtcmd.c_snode 	= LOCALNODE;	
	proc_ptr->p_rmtcmd.c_dnode	= LOCALNODE;
	proc_ptr->p_rmtcmd.c_len	= 0;
	proc_ptr->p_rmtcmd.c_rcode	= OK;

	proc_ptr->p_rmtcmd.c_u.cu_vcopy.v_src	= NONE;
	proc_ptr->p_rmtcmd.c_u.cu_vcopy.v_dst	= NONE;
	proc_ptr->p_rmtcmd.c_u.cu_vcopy.v_rqtr	= NONE;
	proc_ptr->p_rmtcmd.c_u.cu_vcopy.v_saddr	= NULL;
	proc_ptr->p_rmtcmd.c_u.cu_vcopy.v_daddr	= NULL;
	proc_ptr->p_rmtcmd.c_u.cu_vcopy.v_bytes	= 0;

	proc_ptr->p_usr.p_lclsent		= 0;			/* counter of LOCAL sent messages	*/
	proc_ptr->p_usr.p_rmtsent		= 0;			/* counter of REMOTE sent messages	*/

	proc_ptr->p_usr.p_lclcopy		= 0;			/* counter of LOCAL copies as source	*/
	proc_ptr->p_usr.p_rmtcopy		= 0;			/* counter of REMOTE copies as source	*/ 
	
	proc_ptr->p_name_ptr 			= NULL;				
	strcpy(proc_ptr->p_usr.p_name, "$noname");

//	init_timer(&proc_ptr->p_timer);
	proc_ptr->p_task = NULL; 

#if DVSPROFILING != 0
	for( j = 0; j < MAX_PROF; j++)
		proc_ptr->p_profiling[j] = 0;
#endif

}

/*--------------------------------------------------------------*/
/*			flush_receiving_procs			*/
/* A remote node is disconnected: wake up with error all 	*/
/* processes waiting some operation from processes in that node	*/
/* rproxy_ptr must be LOCKED	 				*/
/*--------------------------------------------------------------*/
long flush_receiving_procs(int nodeid, struct proc *rproxy_ptr)
{	
	int v, i;
	dc_desc_t *dc_ptr;
	struct proc *src_ptr, *dst_ptr, *tmp_ptr;
	proc_usr_t *pu_ptr;

DVKDEBUG(INTERNAL,"RPROXY search for process of all DCs waiting an action from a remote process with the node dead\n");
	for( v = 0; v < dvs_ptr->d_nr_dcs; v++) {
		dc_ptr 	= &dc[v];

		RLOCK_DC(dc_ptr);
		if( dc_ptr->dc_usr.dc_flags != 0) {
			RUNLOCK_DC(dc_ptr);
			continue;
		}

		WUNLOCK_PROC(rproxy_ptr);
		LOCK_ALL_PROCS(dc_ptr, tmp_ptr, i);
		WLOCK_PROC(rproxy_ptr);

		FOR_EACH_PROC(dc_ptr, i) {
			tmp_ptr = DC_PROC(dc_ptr,i);

			if( test_bit( BIT_SLOT_FREE, &tmp_ptr->p_usr.p_rts_flags)) 	continue;
			if( IT_IS_REMOTE(tmp_ptr))				continue;
			tmp_ptr->p_rcode = EDVSNOTCONN;
				
			/*A local process is trying to receive a message from a remote process with the node dead */
			do {
				if( test_bit(BIT_RECEIVING, &tmp_ptr->p_usr.p_rts_flags)) {
					if( (tmp_ptr->p_usr.p_getfrom != ANY) 
						&&(tmp_ptr->p_usr.p_getfrom != NONE)){
						src_ptr = ENDPOINT2PTR(dc_ptr, tmp_ptr->p_usr.p_getfrom);
						if( nodeid == src_ptr->p_usr.p_nodeid) {
							pu_ptr = &tmp_ptr->p_usr;
							DVKDEBUG(DBGPROC,"Clean receiving " PROC_USR_FORMAT, PROC_USR_FIELDS(pu_ptr));
							clear_bit(BIT_RECEIVING, &tmp_ptr->p_usr.p_rts_flags);
							if( tmp_ptr->p_usr.p_rts_flags == PROC_RUNNING)
								LOCAL_PROC_UP(tmp_ptr, EDVSNOTCONN);
						}
					}	
				}
			}while(0);
				
			/* A local process has sent a message to a remote process and waiting for the acknowledge but the node dead  */
			do {
				if( test_bit(BIT_SENDING, &tmp_ptr->p_usr.p_rts_flags)) {
					if( (tmp_ptr->p_usr.p_sendto != ANY) 
						&&(tmp_ptr->p_usr.p_sendto != NONE)){
						dst_ptr = ENDPOINT2PTR(dc_ptr, tmp_ptr->p_usr.p_sendto);
						if( nodeid == dst_ptr->p_usr.p_nodeid) {
							pu_ptr = &tmp_ptr->p_usr;
							DVKDEBUG(DBGPROC,"Clean sending " PROC_USR_FORMAT, PROC_USR_FIELDS(pu_ptr));
							clear_bit(BIT_SENDING, &tmp_ptr->p_usr.p_rts_flags);
							if( tmp_ptr->p_usr.p_rts_flags == PROC_RUNNING)
								LOCAL_PROC_UP(tmp_ptr, EDVSNOTCONN);	
						}
					}	
				}
			}while(0);
				
			/* A local process has sent a COPY CMD to a remote process and waiting for the acknowledge but the node dead */
			do {
				if( test_bit(BIT_ONCOPY, &tmp_ptr->p_usr.p_rts_flags)) {
					if(tmp_ptr->p_usr.p_endpoint == tmp_ptr->p_rmtcmd.c_u.cu_vcopy.v_rqtr) {
						if( nodeid == tmp_ptr->p_rmtcmd.c_dnode) {
							pu_ptr = &tmp_ptr->p_usr;
							DVKDEBUG(DBGPROC,"Clean oncopy " PROC_USR_FORMAT, PROC_USR_FIELDS(pu_ptr));
							clear_bit(BIT_ONCOPY, &tmp_ptr->p_usr.p_rts_flags);
							if( tmp_ptr->p_usr.p_rts_flags == PROC_RUNNING) 
								LOCAL_PROC_UP(tmp_ptr, EDVSNOTCONN);
						}	
					}	
				}
			}while(0);			
		}

		WUNLOCK_PROC(rproxy_ptr);
		UNLOCK_ALL_PROCS(dc_ptr, tmp_ptr, i);
		RUNLOCK_DC(dc_ptr);
		WLOCK_PROC(rproxy_ptr);
	}
	return(OK);
}


/*===========================================================================*
 *				init_node		 		     *
 *===========================================================================*/
void init_node(int nodeid)
{
	cluster_node_t *n_ptr;

	n_ptr = &node[nodeid];
	n_ptr->n_usr.n_nodeid 	= nodeid;

	clear_node(n_ptr);

	NODE_LOCK_INIT(n_ptr);
}

/*===========================================================================*
 *				init_proxies		 		     *
 *===========================================================================*/
void init_proxies(int px_nr)
{
	proxies_t *px_ptr;
	struct proc *sproxy_ptr, *rproxy_ptr;
	
	px_ptr = &proxies[px_nr];
	px_ptr->px_usr.px_id = px_nr;
	sproxy_ptr = &px_ptr->px_sproxy;
	rproxy_ptr = &px_ptr->px_rproxy;
	clear_proxies(px_ptr);

	sproxy_ptr->p_pseudosem 	= 1;
	rproxy_ptr->p_pseudosem 	= 1;


	PROXY_LOCK_INIT(px_ptr);

	PROC_LOCK_INIT(sproxy_ptr);
	PROC_LOCK_INIT(rproxy_ptr);

}

/*===========================================================================*
 *				clear_node		 		     *
 *===========================================================================*/
void clear_node(cluster_node_t *n_ptr)
{
	strcpy(n_ptr->n_usr.n_name,"NONAME");
	n_ptr->n_usr.n_flags 	= NODE_FREE;
	n_ptr->n_usr.n_dcs   	= 0;
	n_ptr->n_usr.n_proxies 	= NO_PROXIES;
	n_ptr->n_usr.n_stimestamp.tv_sec = 0;
	n_ptr->n_usr.n_stimestamp.tv_nsec = 0;
	n_ptr->n_usr.n_rtimestamp.tv_sec = 0;
	n_ptr->n_usr.n_rtimestamp.tv_nsec = 0;
	n_ptr->n_usr.n_pxsent 		= 0; 
	n_ptr->n_usr.n_pxrcvd		= 0; 
	
}

/*===========================================================================*
 *				clear_proxies		 					     *
 *===========================================================================*/
void clear_proxies(proxies_t *px_ptr)
{
	struct proc *sproxy_ptr, *rproxy_ptr;

	px_ptr->px_usr.px_flags = PROXIES_FREE;
	px_ptr->px_usr.px_maxbytes = MAXCOPYBUF;
	px_ptr->px_usr.px_maxbcmds = 0;
	sproxy_ptr = &px_ptr->px_sproxy;
	rproxy_ptr = &px_ptr->px_rproxy;

	init_proc_desc(sproxy_ptr, PROXY_NO_DC, px_ptr->px_usr.px_id);
	CLR_SYS_MAP(sproxy_ptr->p_priv.priv_usr.priv_ipc_to);	
	CLR_DVK_MAP(sproxy_ptr->p_priv.priv_usr.priv_dvk_allowed);
	
	init_proc_desc(rproxy_ptr, PROXY_NO_DC, px_ptr->px_usr.px_id);
	CLR_SYS_MAP(rproxy_ptr->p_priv.priv_usr.priv_ipc_to);	
	CLR_DVK_MAP(rproxy_ptr->p_priv.priv_usr.priv_dvk_allowed);

	sproxy_ptr->p_usr.p_rts_flags	= SLOT_FREE;
	rproxy_ptr->p_usr.p_rts_flags	= SLOT_FREE;
	strcpy(px_ptr->px_usr.px_name, "NONAME");

}

/*------------------------------------------------------*/
/*			inherit_cpu				*/
/*------------------------------------------------------*/
void inherit_cpu(struct proc *proc_ptr)
{
	int cpuid, ret;
	cpumask_t tmp_cpumask;
	proc_usr_t *pu_ptr;	
	
	cpuid = get_cpu();
	DVKDEBUG(INTERNAL, "cpuid=%d vpid=%ld\n", cpuid, proc_ptr->p_usr.p_vpid );
	cpumask_clear(&tmp_cpumask);
	cpumask_set_cpu(cpuid, &tmp_cpumask);
	if ( (ret = setaffinity_ptr(proc_ptr->p_usr.p_vpid, &tmp_cpumask)))
		ERROR_PRINT(ret);	
	put_cpu();

	pu_ptr = &proc_ptr->p_usr;
	DVKDEBUG(INTERNAL, PROC_CPU_FORMAT, PROC_CPU_FIELDS(pu_ptr));

}

/****************************************************************
 *  data copy trampoline function
 *  another process command the waiting process to copy data on behalf
*****************************************************************/
int copy_trampoline(struct proc *proc)
{
	int ret;

	DVKDEBUG(INTERNAL,PCOPY_FORMAT, PCOPY_FIELDS(proc)); 
	
	DVKDEBUG(INTERNAL,"proc->rcode=%d\n", proc->p_rcode); 
	switch(proc->p_rcode){
		case EDVSUSR2USR:
			ret = copy_usr2usr(proc->p_tramp.t_src->p_usr.p_endpoint, 
						proc->p_tramp.t_src, proc->p_tramp.t_saddr, 
						proc->p_tramp.t_dst, proc->p_tramp.t_daddr, 
						proc->p_tramp.t_bytes);
			break;
		case EDVSKRN2USR:
			ret = copy_to_user(proc->p_tramp.t_daddr, proc->p_tramp.t_saddr, proc->p_tramp.t_bytes);
			break;
		case EDVSUSR2KRN:
			ret = copy_from_user(proc->p_tramp.t_daddr, proc->p_tramp.t_saddr, proc->p_tramp.t_bytes);
			break;
		default:
			ret = EDVSBADTRAMP;
			break;
	}
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/****************************************************************
 *  Sleep process proc waiting for an event 
 *  On entry: process must be Write LOCKED
 *  On exit: process is Write LOCKED
 *  the result is in proc->p_rcode
*****************************************************************/
int sleep_proc(struct proc *proc, long timeout) 
{
	proc_usr_t *pu_ptr; 
	int rcode = OK;
	int ret = OK;
	sigset_t new_set, old_set;

	DVKDEBUG(INTERNAL,"timeout=%ld\n", timeout); 

	if( timeout == TIMEOUT_NOWAIT) {
		if( test_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags)){
			clear_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags);
			set_bit(BIT_MIGRATE, &proc->p_usr.p_rts_flags);
		}
		proc->p_rcode = EDVSAGAIN; 
		return(EDVSAGAIN);
	}
	
	DVKDEBUG(INTERNAL,"BEFORE DOWN lpid=%d p_sem=%d timeout=%ld\n",proc->p_usr.p_lpid,proc->p_pseudosem, timeout); 
	proc->p_pseudosem = -1; 
	DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags); 

	if(test_bit(MIS_BIT_USERMODE, &proc->p_usr.p_misc_flags)){
		sigaddset(&new_set, SIGALRM);
		sigaddset(&new_set, SIGIO);
		sigaddset(&new_set, SIGTERM);
	//	old_set = current->blocked;
		sigprocmask(SIG_BLOCK, &new_set, &old_set);
		DVKDEBUG(INTERNAL,"new_set.sig[0]=0x%08x old_set.sig[0]=0x%08x\n",
			new_set.sig[0], old_set.sig[0]); 
	}
	
	do {
		proc->p_rcode = OK; 
		WUNLOCK_PROC(proc); 	
		if( timeout < 0) {
			ret = wait_event_interruptible(proc->p_wqhead, (proc->p_pseudosem >= 0));
			ret = (ret == 0)? 1: ret;  
		} else {
			ret = wait_event_interruptible_timeout(proc->p_wqhead, 
				(proc->p_pseudosem >= 0),msecs_to_jiffies(timeout));	
		}
		DVKDEBUG(INTERNAL,"pending: sig[0]:0x%08x, sig[1]:0x%08x\n", 
			current->pending.signal.sig[0], current->pending.signal.sig[1]);
		DVKDEBUG(INTERNAL,"shared_pending sig[0]:0x%08x, sig[1]:0x%08x\n",
			current->signal->shared_pending.signal.sig[0], 
			current->signal->shared_pending.signal.sig[1]);	
#define ONLY_FOR_TEST 1				
			
#ifdef ONLY_FOR_TEST				
		if( sigismember(&current->pending.signal,  SIGALRM) ||
			sigismember(&current->signal->shared_pending.signal, SIGALRM))
			DVKDEBUG(INTERNAL,"SIGALRM received\n");
		if( sigismember(&current->pending.signal,  SIGKILL) ||
			sigismember(&current->signal->shared_pending.signal, SIGKILL))
			DVKDEBUG(INTERNAL,"SIGKILL received\n");
		if( sigismember(&current->pending.signal,  SIGTERM) ||
			sigismember(&current->signal->shared_pending.signal, SIGTERM))
			DVKDEBUG(INTERNAL,"SIGTERM received\n");
#endif // ONLY_FOR_TEST				
		DVKDEBUG(INTERNAL,"endpoint=%d ret=%d p_rcode=%d\n",proc->p_usr.p_endpoint, ret,  proc->p_rcode);
		DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX cpuid=%d\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags, smp_processor_id());  
		WLOCK_PROC(proc); 
		if( proc->p_rcode != EDVSUSR2USR &&
			proc->p_rcode != EDVSKRN2USR && 
			proc->p_rcode != EDVSUSR2KRN ) break;
		ret = copy_trampoline(proc);
		LOCAL_PROC_UP(proc->p_tramp.t_rqtr, ret);
	}while(ret < 0);

	if(test_bit(MIS_BIT_USERMODE, &proc->p_usr.p_misc_flags)){
		sigprocmask(SIG_UNBLOCK, &new_set, &old_set);
		DVKDEBUG(INTERNAL,"new_set.sig[0]=0x%08x old_set.sig[0]=0x%08x\n",
			new_set.sig[0], old_set.sig[0]); 		
	}
	
	if( ret == -ERESTARTSYS){ // a SIGNAL has been received 
		ret = -EINTR;
	}else if( proc->p_rcode < 0) {
		ret = proc->p_rcode;
	} else if( ret < 0) {
		proc->p_rcode = ret;
	} else if (ret == 0){
		if (timeout >= 0 ) 
			ret = EDVSTIMEDOUT;
		proc->p_rcode = ret;
	} else{ /* ret > 0 */
		ret = OK;
		proc->p_rcode = ret;
	}
	
	if( ret) {
		DVKDEBUG(INTERNAL,"pid=%d ret=%d\n",task_pid_nr(current), ret);  
		if(proc->p_pseudosem < 0) proc->p_pseudosem++; 
//		del_timer_sync(&proc->p_timer);
		proc->p_rcode = ret; 
		if( timeout < 0) {
			while(test_bit(BIT_ONCOPY, &proc->p_usr.p_rts_flags)) {
				WUNLOCK_PROC(proc);
				schedule();
				WLOCK_PROC(proc);
			}	
		}
	}

	if( test_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags)){
		clear_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags);
		set_bit(BIT_MIGRATE, &proc->p_usr.p_rts_flags);
	}

	/* reset the process CPU mask */
#ifdef SET_SETAFFINITY 
	if ( (rcode = setaffinity_ptr(proc->p_usr.p_vpid, &proc->p_usr.p_cpumask)))
			ERROR_PRINT(rcode);	
#endif // SET_SETAFFINITY 

	pu_ptr = &proc->p_usr;
	DVKDEBUG(INTERNAL, PROC_CPU_FORMAT, PROC_CPU_FIELDS(pu_ptr));
	
	DVKDEBUG(INTERNAL,"someone wakeups me: sem=%d p_rcode=%d\n",proc->p_pseudosem, proc->p_rcode);
	return(ret);
}

/*--------------------------------------------------------------*/
/*			flush_sending_procs			*/
/* The proxy has finished or the remote node is NOT CONNECTED 	*/
/* wakeup with error all process trying to send a CMD to REMOTE */
/* sproxy_ptr must be LOCKED	 				*/
/*--------------------------------------------------------------*/
long flush_sending_procs(int nodeid,  struct proc *sproxy_ptr)
{
	struct proc *src_ptr, *tmp_ptr;

DVKDEBUG(INTERNAL,"SPROXY wakeup with error all process trying to send a CMD to node=%d\n",  sproxy_ptr->p_usr.p_nodeid);

	LIST_FOR_EACH_ENTRY_SAFE(src_ptr, tmp_ptr, &sproxy_ptr->p_list, p_link) {
		/* RULE TO LOCK: 1st: sender, 2nd: sender proxy */
		WUNLOCK_PROC(sproxy_ptr);
		WLOCK_PROC(src_ptr); 
		if( src_ptr->p_rmtcmd.c_dnode != nodeid) {
			WUNLOCK_PROC(src_ptr);
			WLOCK_PROC(sproxy_ptr);
		 	continue;
		}
		WLOCK_PROC(sproxy_ptr);

		LIST_DEL_INIT(&src_ptr->p_link); /* remove from queue */
		src_ptr->p_usr.p_proxy = NONE;
		clear_bit(BIT_RMTOPER, &src_ptr->p_usr.p_rts_flags);

DVKDEBUG(INTERNAL,"Find process %d trying to send a CMD\n",src_ptr->p_usr.p_endpoint);
		if( IT_IS_REMOTE(src_ptr)) {	/* Acknowledges to remote process */		
			src_ptr->p_usr.p_rts_flags = REMOTE; 
			src_ptr->p_usr.p_getfrom = NONE;
			src_ptr->p_usr.p_sendto = NONE;
		} else {
DVKDEBUG(INTERNAL,"Wakeup SENDER with error ep=%d  pid=%d\n",src_ptr->p_usr.p_endpoint, src_ptr->p_usr.p_lpid);	
			switch(src_ptr->p_rmtcmd.c_cmd) {
				case CMD_SNDREC_MSG:
					clear_bit(BIT_RECEIVING, &src_ptr->p_usr.p_rts_flags);
					src_ptr->p_usr.p_getfrom = NONE;
				case CMD_SEND_MSG:
				case CMD_NTFY_MSG:
				case CMD_REPLY_MSG:
					clear_bit(BIT_SENDING, &src_ptr->p_usr.p_rts_flags);
					src_ptr->p_usr.p_sendto = NONE;
					break;
				case CMD_COPYIN_DATA:
				case CMD_COPYIN_RQST:
				case CMD_COPYLCL_RQST:
				case CMD_COPYRMT_RQST:
					if(test_bit(BIT_ONCOPY, &src_ptr->p_usr.p_rts_flags)){
						/* Only the requester of a VCOPY CMD must be waked up */
						if( src_ptr->p_usr.p_endpoint == src_ptr->p_rmtcmd.c_u.cu_vcopy.v_rqtr) 
							clear_bit(BIT_ONCOPY, &src_ptr->p_usr.p_rts_flags);
					}
					break;
				default:
					WUNLOCK_PROC(src_ptr); 
					ERROR_RETURN(EDVSBADREQUEST);			
					break;
				}
			if(src_ptr->p_usr.p_rts_flags == 0) 
				LOCAL_PROC_UP(src_ptr, EDVSNOTCONN);
		}
		WUNLOCK_PROC(src_ptr); 
	}
	return(OK);
}

/*===========================================================================*
 *				bm2ascii		 			   				     *
 *===========================================================================*/
void bm2ascii(char *buf, unsigned long int bitmap)
{
	int i;
	unsigned long int mask;

	mask = 0x80000000; 
	for( i = 0; i < BITMAP_32BITS ; i++) {
		*buf++ = (bitmap & mask)?'X':'-';
		mask =  (mask >> 1);		
	}
	*buf = '\0';
}

/// PONER EN com/macros.h
#define GET_SYS_BIT(map,bit) ( MAP_CHUNK(map->chunk,bit) & (1 << CHUNK_OFFSET(bit) )

/*===========================================================================*
 *				ipcmap2ascii		 			   				     *
 *===========================================================================*/
void ipcmap2ascii(char *buf, ipc_map_t *map_ptr)
{
	int i,j;

	for(i = NR_SYS_CHUNKS-1; i >= 0; i--){
		for( j = (BITCHUNK_BITS-1); j >= 0 ; j--, buf++) {
			if (GET_SYS_BIT(map_ptr, (j + (i * BITCHUNK_BITS)))) 
				*buf = 'X';
			else 
				*buf = '-';
		}
	}
}

/*===========================================================================*
 *				dvkcalls2ascii		 			   				     *
 *===========================================================================*/
void dvkcalls2ascii(char *buf, dvk_map_t *map_ptr)
{
	int i,j;

	for(i = NR_DVK_CHUNKS-1; i >= 0; i--){
		for( j = (BITCHUNK_BITS-1); j >= 0 ; j--, buf++) {
			if (GET_SYS_BIT(map_ptr, (j + (i * BITCHUNK_BITS)))) 
				*buf = 'X';
			else 
				*buf = '-';
		}
	}
}

/*--------------------------------------------------------------*/
/*			check_caller			*/
/* Checks if the caller is a thread. Checks if it is		*/
/* binded or its main thread is binded				*/
/* ON OUTPUT: 							*/
/*   if ret==OK; 			*/			
/*	*c_ptr = caller_ptr;					*/
/*	*t_ptr = task_ptr;					*/
/*	*c_pid = caller_pid;					*/
/*--------------------------------------------------------------*/

int check_caller(struct task_struct **t_ptr, struct proc **c_ptr, int *c_pid)
{
	int ret, px, dcid;
	struct task_struct *task_ptr;
	struct proc *caller_ptr;
	int  caller_pid, caller_tgid;
	dc_desc_t *dc_ptr;
	proc_usr_t  *up_ptr;
	
	task_ptr = current;

	caller_pid  = task_pid_nr(current);
	caller_tgid = current->tgid;
	DVKDEBUG(DBGPARAMS,"caller_pid=%d caller_tgid=%d\n", 
		caller_pid, caller_tgid);
	ret = OK;
	*t_ptr = task_ptr;
	do {
//		if(caller_pid == caller_tgid) {	/* task_ptr it is a MAIN thread 	*/
		if( thread_group_leader(task_ptr)){ /* Caller is the Group Leader */
			caller_ptr = (struct proc *) task_ptr->task_proc;
			if( caller_ptr == NULL){		/* The main thread is not binded 	*/
				task_ptr = current;			
				ret = EDVSNOTBIND;
			}
			*t_ptr = task_ptr;
		}else {					/* Caller is NOT the Group Leader */	
			caller_ptr = (struct proc *)current->task_proc;
			if( caller_ptr == NULL){			/* The child thread is not binded */
				task_ptr= task_ptr->group_leader;
				if( task_ptr == NULL) {			/* the Group Leader has dead */
					task_ptr = current;
					ret = EDVSNOTBIND;	
					break;
				}
				RLOCK_TASK(task_ptr);	
				caller_ptr = (struct proc *) task_ptr->task_proc;
				if(caller_ptr == NULL){			/* The main thread is not binded */
					ret = EDVSNOTBIND;
				}else{
					caller_pid = task_ptr->tgid;	/* Use the main thread binding	*/	
				}
				*t_ptr = task_ptr;
				RUNLOCK_TASK(task_ptr);	
			}else{
				*t_ptr = task_ptr;	
			}
		}
	}while(0);

	/* The DC administrator has set the need to migrate for this process */
	if( ret)  ERROR_RETURN(ret);

    if( caller_ptr == NULL) ERROR_PRINT(EDVSINVAL);
	
	WLOCK_PROC(caller_ptr);
	dcid	= caller_ptr->p_usr.p_dcid;
	
	/* Check to see if the process has been marked to migrate  */
	up_ptr = &caller_ptr->p_usr;
	DVKDEBUG(INTERNAL,PROC_USR_FORMAT,PROC_USR_FIELDS(up_ptr));
	px = test_bit(MIS_BIT_PROXY, &caller_ptr->p_usr.p_misc_flags);

	if( !px){ // NOT A PROXY 
		if(! test_bit(MIS_BIT_NOMIGRATE, &caller_ptr->p_usr.p_misc_flags)){
			if( test_bit(MIS_BIT_NEEDMIGR, &caller_ptr->p_usr.p_misc_flags)){
				up_ptr = &caller_ptr->p_usr;
				DVKDEBUG(INTERNAL,PROC_USR_FORMAT,PROC_USR_FIELDS(up_ptr));
				clear_bit(MIS_BIT_NEEDMIGR, &caller_ptr->p_usr.p_misc_flags);
				set_bit(BIT_MIGRATE, &caller_ptr->p_usr.p_rts_flags);
				/* stop local processing */
			}
			
			/* The process  sleep to migrate  , if it is not a REMOTE **/
			/* during a process migration to the local_nodeid the process may be REMOTE */
			if( test_bit(BIT_MIGRATE, &caller_ptr->p_usr.p_rts_flags) 
				&& !test_bit(BIT_REMOTE, &caller_ptr->p_usr.p_rts_flags) ){
				sleep_proc(caller_ptr, TIMEOUT_FOREVER);
				ret = caller_ptr->p_rcode;
				if(ret) 
					ERROR_WUNLOCK_PROC(caller_ptr, ret);
			}
		}
		
		/* If the process is not a REMOTE BACKUP, it must be in RUNNING state */
		if ( !test_bit(MIS_BIT_RMTBACKUP, &caller_ptr->p_usr.p_misc_flags) ) {
			if (caller_ptr->p_usr.p_rts_flags) {
				up_ptr = &caller_ptr->p_usr;
				DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(up_ptr));
				ERROR_WUNLOCK_PROC(caller_ptr,EDVSPROCRUN);
			}
		} else { // A Remote Backup process must be in not RUNNING state 
			if (!test_bit(BIT_REMOTE, &caller_ptr->p_usr.p_rts_flags)) {
				up_ptr = &caller_ptr->p_usr;
				DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(up_ptr));
				ERROR_WUNLOCK_PROC(caller_ptr,EDVSPROCRUN);
			}
		}
	}
	
	// if process is REMOTE BACKUP, wait until it will be the PRIMARY (active LOCAL endpoint)
	while( test_bit(BIT_REMOTE, &caller_ptr->p_usr.p_rts_flags) &&
		test_bit(MIS_BIT_RMTBACKUP, &caller_ptr->p_usr.p_misc_flags)) {
		set_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_waitmigr = caller_ptr->p_usr.p_endpoint;
		INIT_LIST_HEAD(&caller_ptr->p_mlink);
		LIST_ADD_TAIL(&caller_ptr->p_mlink, &caller_ptr->p_mlist);
		sleep_proc(caller_ptr, TIMEOUT_MOLCALL);
		ret = caller_ptr->p_rcode;
		if( ret == OK) continue;
		LIST_DEL_INIT(&caller_ptr->p_mlink);
		clear_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_waitmigr = NONE;
		if( ret != EDVSTIMEDOUT ){
			WUNLOCK_PROC(caller_ptr);
			ERROR_RETURN(ret);
		}
		ERROR_PRINT(ret);
	}
	
	WUNLOCK_PROC(caller_ptr);
	
	if( !px ) {  /* It is not a proxy */
		DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
		if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs) 	
			ERROR_RETURN(EDVSBADDCID);
		dc_ptr = &dc[dcid];
		RLOCK_DC(dc_ptr);
		ret = OK;
		if( dc_ptr->dc_usr.dc_flags)  
			ret = EDVSDCNOTRUN;
		RUNLOCK_DC(dc_ptr);
		if( ret) ERROR_RETURN(ret);
	}
	
	*c_ptr = caller_ptr;
	*c_pid = caller_pid;
	DVKDEBUG(DBGPARAMS,"caller_pid=%d \n", caller_pid);

	ret = OK;
	RLOCK_PROC(caller_ptr);
	if ( test_bit(MIS_BIT_RMTBACKUP, &caller_ptr->p_usr.p_misc_flags) ) {
		ret = EDVSRMTPROC;
	}
	RUNLOCK_PROC(caller_ptr);

	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/*--------------------------------------------------------------*/
/*			unlock_sr_proxies				*/
/* the proxy structure MUST BE LOCKED				*/
/* unlock both proxy processes					*/
/* if sender = receiver only unlocks sender			*/
/* if error, none is unlocked					*/
/*--------------------------------------------------------------*/
int unlock_sr_proxies(int px_nr)
{
	struct proc *sproxy_ptr, *rproxy_ptr;
	proxies_t *px_ptr;

	DVKDEBUG(DBGPARAMS,"px_nr=%d\n", px_nr);

//	if(get_current_cred()->euid.val != USER_ROOT) ERROR_RETURN(EDVSPRIVILEGES);

	if( DVS_NOT_INIT() )   			ERROR_RETURN(EDVSDVSINIT );

	CHECK_PROXYID(px_nr); 

	px_ptr = &proxies[px_nr];
	if( px_ptr->px_usr.px_flags == PROXIES_FREE)	{
		ERROR_RETURN(EDVSNOPROXY);
	}

	sproxy_ptr = &px_ptr->px_sproxy; 
	rproxy_ptr = &px_ptr->px_rproxy; 

	DVKDEBUG(DBGPARAMS,"sproxy_pid=%d, rproxy_pid=%d\n", sproxy_ptr->p_usr.p_lpid, rproxy_ptr->p_usr.p_lpid);

	WUNLOCK_PROC(sproxy_ptr);
	WUNLOCK_PROC(rproxy_ptr);
	WUNLOCK_PROXY(px_ptr);

	return(OK);
}

/*--------------------------------------------------------------*/
/*			lock_sr_proxies				*/
/* get the process prointer of sender and receiver proxies 	*/
/* and lock both processes and the proxy structure 		*/
/* if sender = receiver only locks sender			*/
/* if error, none is locked					*/
/*--------------------------------------------------------------*/
int lock_sr_proxies(int px_nr,  struct proc **spxy_ptr,  struct proc **rpxy_ptr)
{
	proxies_t *px_ptr;
	struct proc *sproxy_ptr, *rproxy_ptr;

	DVKDEBUG(DBGPARAMS,"px_nr=%d\n", px_nr);

//	if(get_current_cred()->euid.val != USER_ROOT) ERROR_RETURN(EDVSPRIVILEGES);

	if( DVS_NOT_INIT() )   			ERROR_RETURN(EDVSDVSINIT );
	CHECK_PROXYID(px_nr); 

	px_ptr = &proxies[px_nr];
	WLOCK_PROXY(px_ptr);
	if( px_ptr->px_usr.px_flags == PROXIES_FREE)	{
		WUNLOCK_PROXY(px_ptr);
		ERROR_RETURN(EDVSPROXYFREE);
	}

	sproxy_ptr = &px_ptr->px_sproxy; 
	rproxy_ptr = &px_ptr->px_rproxy; 
	WLOCK_PROC(rproxy_ptr);
	WLOCK_PROC(sproxy_ptr);

	DVKDEBUG(DBGPARAMS,"sproxy_pid=%d, rproxy_pid=%d\n", sproxy_ptr->p_usr.p_lpid, rproxy_ptr->p_usr.p_lpid);

	*spxy_ptr = sproxy_ptr;
	*rpxy_ptr = rproxy_ptr;

	return(OK);
}

/****************************************************************
 *  Sleep process proc waiting for an event 
 *  On entry: processes proc and other must be Write LOCKED
 *  On exit: processes proc and other must be Write LOCKED
 *  the result is in proc->p_rcode
*****************************************************************/
int sleep_proc2(struct proc *proc, struct proc *other , long timeout)  
{
	proc_usr_t *pu_ptr; 
	int ret = OK;
	int rcode = OK;
	sigset_t	new_set, old_set;
	int proc_nr, other_nr;

	if( timeout == TIMEOUT_NOWAIT) {
		if( test_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags)){
			clear_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags);
			set_bit(BIT_MIGRATE, &proc->p_usr.p_rts_flags);
		}
		proc->p_rcode = EDVSAGAIN;
		return(EDVSAGAIN);
	}
	
	DVKDEBUG(INTERNAL,"BEFORE DOWN lpid=%d p_sem=%d timeout=%ld\n",proc->p_usr.p_lpid,proc->p_pseudosem, timeout); 
	proc->p_pseudosem = -1; 
	DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags); 
	
	if(test_bit(MIS_BIT_USERMODE, &proc->p_usr.p_misc_flags)){
		sigaddset(&new_set, SIGALRM);
		sigaddset(&new_set, SIGIO);
		//old_set = current->blocked;
		 sigprocmask(SIG_BLOCK, &new_set, &old_set);
		DVKDEBUG(INTERNAL,"new_set.sig[0]=0x%08x old_set.sig[0]=0x%08x\n",
			new_set.sig[0], old_set.sig[0]); 
	}

	proc_nr = proc->p_usr.p_nr;
	other_nr= other->p_usr.p_nr;
	
	do {
		proc->p_rcode = OK; 
		WUNLOCK_PROC2(proc, other); 
		DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags); 

		if( timeout < 0) {
			ret = wait_event_interruptible(proc->p_wqhead, (proc->p_pseudosem >= 0));
			ret = (ret == 0)? 1: ret;  
		} else {
			ret = wait_event_interruptible_timeout(proc->p_wqhead, 
				(proc->p_pseudosem >= 0),msecs_to_jiffies(timeout));
		}
		DVKDEBUG(INTERNAL,"pending: sig[0]:0x%08x, sig[1]:0x%08x\n", 
			current->pending.signal.sig[0], current->pending.signal.sig[1]);
		DVKDEBUG(INTERNAL,"shared_pending sig[0]:0x%08x, sig[1]:0x%08x\n",
			current->signal->shared_pending.signal.sig[0], 
			current->signal->shared_pending.signal.sig[1]);	
#ifdef ONLY_FOR_TEST							
		if( sigismember(&current->pending.signal,  SIGTERM) ||
			sigismember(&current->signal->shared_pending.signal, SIGTERM))
			DVKDEBUG(INTERNAL,"SIGTERM received\n");		
#endif //ONLY_FOR_TEST				

		DVKDEBUG(INTERNAL,"endpoint=%d ret=%d p_rcode=%d\n",proc->p_usr.p_endpoint, ret,  proc->p_rcode);
		DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX cpuid=%d\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags, smp_processor_id());  

		WLOCK_ORDERED2(proc_nr, other_nr, proc, other); 
		if( proc->p_rcode != EDVSUSR2USR &&
			proc->p_rcode != EDVSKRN2USR && 
			proc->p_rcode != EDVSUSR2KRN ) break;
		ret = copy_trampoline(proc);
		LOCAL_PROC_UP(proc->p_tramp.t_rqtr, ret);
	}while(ret < 0);
	
	if(test_bit(MIS_BIT_USERMODE, &proc->p_usr.p_misc_flags)){
		sigprocmask(SIG_UNBLOCK, &new_set, &old_set);
		DVKDEBUG(INTERNAL,"new_set.sig[0]=0x%08x old_set.sig[0]=0x%08x\n",
			new_set.sig[0], old_set.sig[0]); 		 
	}
	
	if( ret == -ERESTARTSYS){
		ret = -EINTR;
	}else if( proc->p_rcode < 0) {
		ret = proc->p_rcode;
	}else if( ret < 0) {
		proc->p_rcode = ret;
	}else if (ret == 0){
		if (timeout >= 0 ) ret = EDVSTIMEDOUT;
		proc->p_rcode = ret;
	}else{ /* ret > 0 */
		ret = OK;
		proc->p_rcode = ret;
	}
		
	if( ret) {
		DVKDEBUG(INTERNAL,"pid=%d ret=%d\n",task_pid_nr(current), ret);  
		if(proc->p_pseudosem < 0) proc->p_pseudosem++; 
//		del_timer_sync(&proc->p_timer);
		proc->p_rcode = ret; 
		if( timeout < 0) {
			while(test_bit(BIT_ONCOPY, &proc->p_usr.p_rts_flags)) {
				WUNLOCK_PROC2(proc, other);
				schedule();
				WLOCK_ORDERED2(proc_nr, other_nr,  proc, other);
			}	
		}
	}

	if( test_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags)){
		clear_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags);
		set_bit(BIT_MIGRATE, &proc->p_usr.p_rts_flags);
	}
	
	/* reset the process CPU mask */
#ifdef SET_SETAFFINITY 
	if ( (rcode = setaffinity_ptr(proc->p_usr.p_vpid, &proc->p_usr.p_cpumask)))
			ERROR_PRINT(rcode);	
#endif // SET_SETAFFINITY 
			
	pu_ptr = &proc->p_usr;
	DVKDEBUG(INTERNAL, PROC_CPU_FORMAT, PROC_CPU_FIELDS(pu_ptr));
	
	DVKDEBUG(INTERNAL,"someone wakeups me: sem=%d p_rcode=%d\n",proc->p_pseudosem, proc->p_rcode);
	return(ret);
}

int sleep_proc3(struct proc *proc, struct proc *other1, struct proc *other2 , long timeout) 
{
	proc_usr_t *pu_ptr; 
	int ret = OK;
	int rcode = OK;
	sigset_t	new_set, old_set;
    int proc_nr, other1_nr, other2_nr; 
	
	if( timeout == TIMEOUT_NOWAIT) {
		if( test_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags)){
			clear_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags);
			set_bit(BIT_MIGRATE, &proc->p_usr.p_rts_flags);
		}
		proc->p_rcode = EDVSAGAIN; 
		return(EDVSAGAIN);
	}
	
	DVKDEBUG(INTERNAL,"BEFORE DOWN lpid=%d p_sem=%d timeout=%ld\n",proc->p_usr.p_lpid,proc->p_pseudosem, timeout); 
	proc->p_pseudosem = -1; 
	DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags); 
	
	if(test_bit(MIS_BIT_USERMODE, &proc->p_usr.p_misc_flags)){
		sigaddset(&new_set, SIGALRM);
		sigaddset(&new_set, SIGIO);
		//old_set = current->blocked;
		 sigprocmask(SIG_BLOCK, &new_set, &old_set);
		DVKDEBUG(INTERNAL,"new_set.sig[0]=0x%08x old_set.sig[0]=0x%08x\n",
			new_set.sig[0], old_set.sig[0]); 
	}
	proc_nr = proc->p_usr.p_nr;
	other1_nr = other1->p_usr.p_nr;
	other2_nr = other2->p_usr.p_nr;
	do {
		proc->p_rcode = OK; 
		WUNLOCK_PROC3(proc, other1, other2); 
		DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags); 

		if( timeout < 0) {
			ret = wait_event_interruptible(proc->p_wqhead, (proc->p_pseudosem >= 0));
			ret = (ret == 0)? 1: ret;  
		} else {
			ret = wait_event_interruptible_timeout(proc->p_wqhead, 
				(proc->p_pseudosem >= 0),msecs_to_jiffies(timeout));
		}
		DVKDEBUG(INTERNAL,"pending: sig[0]:0x%08x, sig[1]:0x%08x\n", 
			current->pending.signal.sig[0], current->pending.signal.sig[1]);
		DVKDEBUG(INTERNAL,"shared_pending sig[0]:0x%08x, sig[1]:0x%08x\n",
			current->signal->shared_pending.signal.sig[0], 
			current->signal->shared_pending.signal.sig[1]);	
#ifdef ONLY_FOR_TEST							
		if( sigismember(&current->pending.signal,  SIGTERM) ||
			sigismember(&current->signal->shared_pending.signal, SIGTERM))
			DVKDEBUG(INTERNAL,"SIGTERM received\n");		
#endif //ONLY_FOR_TEST				

		DVKDEBUG(INTERNAL,"endpoint=%d ret=%d p_rcode=%d\n",proc->p_usr.p_endpoint, ret,  proc->p_rcode);
		DVKDEBUG(INTERNAL,"endpoint=%d flags=%lX cpuid=%d\n",proc->p_usr.p_endpoint, proc->p_usr.p_rts_flags, smp_processor_id());  

		WLOCK_ORDERED3(proc_nr, other1_nr, other2_nr, proc, other1, other2); 
		if( proc->p_rcode != EDVSUSR2USR &&
			proc->p_rcode != EDVSKRN2USR && 
			proc->p_rcode != EDVSUSR2KRN ) break;
		ret = copy_trampoline(proc);
		LOCAL_PROC_UP(proc->p_tramp.t_rqtr, ret);
	}while(ret < 0);
	
	if(test_bit(MIS_BIT_USERMODE, &proc->p_usr.p_misc_flags)){
		sigprocmask(SIG_UNBLOCK, &new_set, &old_set);
		DVKDEBUG(INTERNAL,"new_set.sig[0]=0x%08x old_set.sig[0]=0x%08x\n",
			new_set.sig[0], old_set.sig[0]); 
	}
	
	if( ret == -ERESTARTSYS){
		ret = -EINTR;
	}else if( proc->p_rcode < 0) {
		ret = proc->p_rcode;
	}else if( ret < 0) {
		proc->p_rcode = ret;
	}else if (ret == 0){
		if (timeout >= 0 ) ret = EDVSTIMEDOUT;
		proc->p_rcode = ret;
	}else{ /* ret > 0 */
		ret = OK;
		proc->p_rcode = ret;
	}
		
	if( ret) {
		DVKDEBUG(INTERNAL,"pid=%d ret=%d\n",task_pid_nr(current), ret);  
		if(proc->p_pseudosem < 0) proc->p_pseudosem++; 
//		del_timer_sync(&proc->p_timer);
		proc->p_rcode = ret; 
		if( timeout < 0) {
			while(test_bit(BIT_ONCOPY, &proc->p_usr.p_rts_flags)) {
				WUNLOCK_PROC3(proc, other1, other2); 
				schedule();
				WLOCK_ORDERED3(proc_nr, other1_nr, other2_nr, proc, other1, other2); 
			}	
		}
	}

	if( test_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags)){
		clear_bit(MIS_BIT_NEEDMIGR, &proc->p_usr.p_misc_flags);
		set_bit(BIT_MIGRATE, &proc->p_usr.p_rts_flags);
	}
	
	/* reset the process CPU mask */
#ifdef SET_SETAFFINITY 
	if ( (rcode = setaffinity_ptr(proc->p_usr.p_vpid, &proc->p_usr.p_cpumask)))
			ERROR_PRINT(rcode);	
#endif // SET_SETAFFINITY 
			
	pu_ptr = &proc->p_usr;
	DVKDEBUG(INTERNAL, PROC_CPU_FORMAT, PROC_CPU_FIELDS(pu_ptr));
	
	DVKDEBUG(INTERNAL,"someone wakeups me: sem=%d p_rcode=%d\n",proc->p_pseudosem, proc->p_rcode);
	return(ret);
}

/****************************************************************
 *  Copy a block of data from Userspace to Userspace	
*****************************************************************/
				 
long copy_usr2usr(int rqtr_ep, struct proc *src_ptr, char __user *src_addr, 
				struct proc *dst_ptr,  char __user *dst_addr, int bytes)
{
#define RW_WRITE 1
#define RW_READ  0
	struct proc *rqtr_ptr;
	int ret = OK;
	int len;
	long timeout;
	struct iovec lvec[1], rvec[1];
	int rqtr_nr, src_nr, dst_nr; 
	
	DVKDEBUG(DBGPARAMS,"rqtr_ep=%d src_ep=%d src_lpid=%d src_vpid=%d src_addr=%p\n",
		rqtr_ep,src_ptr->p_usr.p_endpoint, src_ptr->p_usr.p_lpid, src_ptr->p_usr.p_vpid, src_addr);
	
	DVKDEBUG(DBGPARAMS,"dst_ep=%d dst_lpid=%d dst_vpid=%d dst_addr=%p bytes=%d\n", 
		dst_ptr->p_usr.p_endpoint, dst_ptr->p_usr.p_lpid, dst_ptr->p_usr.p_vpid, dst_addr, bytes);
 
	if( bytes < 0 || bytes  > MAXCOPYLEN) ERROR_RETURN(EDVSRANGE);
	
	DVKDEBUG(DBGPARAMS,"task_pid_nr(current)=%ld\n", task_pid_nr(current));

	if( task_pid_nr(current) == src_ptr->p_usr.p_lpid){ // WRITE
		DVKDEBUG(INTERNAL,"WRITE\n");
		lvec[0].iov_base = src_addr;
		lvec[0].iov_len  = bytes;
		rvec[0].iov_base = dst_addr;
		rvec[0].iov_len  = bytes;
		DVKDEBUG(DBGPARAMS,"task_pid_nr(dst_ptr->p_task)=%ld\n", task_pid_nr(dst_ptr->p_task));
		len = dvk_vm_rw(dst_ptr->p_task, lvec, 1, rvec, 1, 0, RW_WRITE);
	} else if ( task_pid_nr(current) == dst_ptr->p_usr.p_lpid){ // READ 
		DVKDEBUG(INTERNAL,"READ\n");
		rvec[0].iov_base = src_addr;
		rvec[0].iov_len  = bytes;
		lvec[0].iov_base = dst_addr;
		lvec[0].iov_len  = bytes;
		DVKDEBUG(DBGPARAMS,"task_pid_nr(src_ptr->p_task)=%ld\n", task_pid_nr(src_ptr->p_task));
		len = dvk_vm_rw(src_ptr->p_task, lvec, 1, rvec, 1, 0, RW_READ);
	} else { 
		// Fill copy trampoline fields of the process that will do the copy 
		DVKDEBUG(INTERNAL,"COPY3 bytes=%d\n", bytes);

		if( thread_group_leader(current)){ /* Caller is the Group Leader */
			rqtr_ptr = current->task_proc;
		}else{
			rqtr_ptr = current->group_leader->task_proc;			
		}

		BUG_ON(src_ptr == NULL);
		BUG_ON(dst_ptr == NULL);
		BUG_ON(rqtr_ptr == NULL);
		BUG_ON(src_addr == NULL);
		BUG_ON(dst_addr == NULL);
		
		src_ptr->p_tramp.t_src   = src_ptr;
		src_ptr->p_tramp.t_dst   = dst_ptr;
		src_ptr->p_tramp.t_rqtr  = rqtr_ptr;
		src_ptr->p_tramp.t_saddr  = src_addr;
		src_ptr->p_tramp.t_daddr  = dst_addr;
		
		src_ptr->p_tramp.t_bytes = bytes;
		DVKDEBUG(INTERNAL,PCOPY_FORMAT,PCOPY_FIELDS(src_ptr));
		
		// wake up the copy process 
		LOCAL_PROC_UP( src_ptr, EDVSUSR2USR);
		
		// requester process must sleep until the copy was done 
		rqtr_ptr->p_pseudosem = -1; 
		rqtr_ptr->p_rcode = 0; 
		timeout = TIMEOUT_FOREVER;
		rqtr_nr = rqtr_ptr->p_usr.p_nr;
		src_nr = src_ptr->p_usr.p_nr;
		dst_nr = dst_ptr->p_usr.p_nr;

		WUNLOCK_PROC3(rqtr_ptr,src_ptr,dst_ptr);
		if( timeout < 0) {
			ret = wait_event_interruptible(rqtr_ptr->p_wqhead, (rqtr_ptr->p_pseudosem >= 0));
			ret = (ret == 0)? 1: ret;  
		} else {
			ret = wait_event_interruptible_timeout(rqtr_ptr->p_wqhead, 
				(rqtr_ptr->p_pseudosem >= 0),msecs_to_jiffies(timeout));	
		}
		if( ret == -ERESTARTSYS) ret = -EINTR;
		WLOCK_ORDERED3(rqtr_nr,src_nr,dst_nr,rqtr_ptr,src_ptr,dst_ptr);
		len = rqtr_ptr->p_rcode;
	}
	
	DVKDEBUG(INTERNAL,"len=%d\n",len);
	return(len);
}

long copy_krn2usr(int source, char *src_addr, struct proc *dst_proc, char *dst_addr, int bytes)
{
	DVKDEBUG(INTERNAL,"FUNCTION NOT MIGRATED YET\n");
	ERROR_RETURN(EDVSNOSYS)
}

long copy_usr2krn(int source, struct proc *src_proc, char *src_addr, char *dst_addr, int bytes)
{
	DVKDEBUG(INTERNAL,"FUNCTION NOT MIGRATED YET\n");
	ERROR_RETURN(EDVSNOSYS)
}
	
//################################################################
//################################################################
//################################################################
//########################## AQUI  #################################
//################################################################
//################################################################
//################################################################
#ifdef 	NULL_CODE

asmlinkage int copy_pte_range(struct mm_struct *dst_mm, struct mm_struct *src_mm,
		pmd_t *dst_pmd, pmd_t *src_pmd, struct vm_area_struct *vma,
		unsigned long addr, unsigned long end);

	

#ifdef UNUSED
		
proc_t *get_sproxy(int nodeid) 
{

	cluster_node_t *n_ptr;
	proxies_t *px_ptr;
	proc_t *sproxy_ptr;

	n_ptr = &node[nodeid];
	RLOCK_NODE(n_ptr);
	px_ptr = &proxies[n_ptr->n_usr.n_proxies];
	RUNLOCK_NODE(n_ptr);

	RLOCK_PROXY(px_ptr);
	sproxy_ptr = &px_ptr->px_sproxy;
	RUNLOCK_PROXY(px_ptr);
	
	return(sproxy_ptr);
} 
		
proc_t *get_rproxy(int nodeid) 
{

	cluster_node_t *n_ptr;
	proxies_t *px_ptr;
	proc_t *rproxy_ptr;

	n_ptr = &node[nodeid];
	RLOCK_NODE(n_ptr);
	px_ptr = &proxies[n_ptr->n_usr.n_proxies];
	RUNLOCK_NODE(n_ptr);

	RLOCK_PROXY(px_ptr);
	rproxy_ptr = &px_ptr->px_rproxy;
	RUNLOCK_PROXY(px_ptr);
	
	return(rproxy_ptr);
} 
#endif /* UNUSED*/



/****************************************************************
 *  Copy a block of data from kernel thread to other process Userspace	
 *!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *  SOURCE AND DESTINATION MUST BE ALIGNED !!!!
*****************************************************************/

long copy_krn2usr(int source, char *src_addr, struct proc *dst_proc, char *dst_addr, int bytes)
{
	struct page *dst_pg;
	void *daddr = NULL;
	unsigned long src_off;
	unsigned long dst_off;
	int 	src_npag, dst_npag;	/* number of pages the  copy implies*/
	int ret = OK;
	int len, slen, dlen;
	message *m_ptr;

DVKDEBUG(DBGPARAMS,"source=%d dst_pid=%d bytes=%d\n", 
		source, dst_proc->p_usr.p_lpid, bytes);
 
	if( bytes < 0 || bytes  > MAXCOPYLEN) ERROR_RETURN(EDVSRANGE);


	src_off  = (long int) src_addr & (~PAGE_MASK);
	dst_off  = (long int) dst_addr & (~PAGE_MASK);
DVKDEBUG(INTERNAL,"src_off=%ld dst_off=%ld\n",src_off, dst_off);

	src_npag = (src_off+bytes+PAGE_SIZE-1)>>PAGE_SHIFT;
	dst_npag = (dst_off+bytes+PAGE_SIZE-1)>>PAGE_SHIFT;
DVKDEBUG(INTERNAL,"src_npag=%d dst_npag=%d\n",src_npag, dst_npag);
    if( src_npag != dst_npag ) ERROR_RETURN(EDVSALIGN);
	// buffer (not messages) must be alligned 
    if( src_npag != 1 
		&&  (src_off != dst_off)
		&&  (bytes != sizeof(message))) ERROR_RETURN(EDVSALIGN);
	
	while( bytes > 0) {

		down_read(&dst_proc->p_task->mm->mmap_sem);
		ret = get_user_pages(dst_proc->p_task, dst_proc->p_task->mm,
     	         	(unsigned long)dst_addr, 1, 1, 0, &dst_pg, NULL);
		up_read(&dst_proc->p_task->mm->mmap_sem);
		if (ret != 1) ERROR_RETURN(EDVSADDRNOTAVAIL);
DVKDEBUG(INTERNAL,"get_user_pages DST OK\n");

		daddr = kmap_atomic(dst_pg, KM_USER0);
DVKDEBUG(INTERNAL,"kmap_atomic DST OK\n");

		slen = PAGE_SIZE-src_off;
		dlen = PAGE_SIZE-dst_off;
		len = min(slen,dlen);
		len = min(len,bytes);

		if( len == PAGE_SIZE) {
	    		copy_page(daddr , src_addr);
DVKDEBUG(INTERNAL,"copy_page %d bytes\n", len);

		}else{
	    		memcpy((daddr + dst_off), (src_addr + src_off), len);
DVKDEBUG(INTERNAL,"memcpy %d bytes\n", len);
		}

		/*fill the message sender field */
DVKDEBUG(INTERNAL,"source=%d bytes=%d sizeof(message)=%d\n", source, bytes, sizeof(message));
		if( (source != NONE) && (bytes == sizeof(message))) {
DVKDEBUG(INTERNAL,"source=%d bytes=%d\n", source, bytes);
			m_ptr = (message *) (daddr + dst_off);
			m_ptr->m_source = source;
		}

		kunmap_atomic(daddr, KM_USER0);
DVKDEBUG(INTERNAL,"kunmap_atomic DST OK\n");
		set_page_dirty_lock(dst_pg);
DVKDEBUG(INTERNAL,"set_page_dirty_lock OK\n");
     	put_page(dst_pg);
DVKDEBUG(GENERIC,"put_page DST OK\n");

		src_addr+=len;
		dst_addr+=len;		
		src_off  = (long int) src_addr & (~PAGE_MASK);
		dst_off  = (long int) dst_addr & (~PAGE_MASK);
DVKDEBUG(INTERNAL,"src_off=%ld dst_off=%ld\n",src_off, dst_off);
		bytes=bytes-len;
	}

return(OK);
}

/****************************************************************
 *  Copy a block of data from other Userspace to kernel thread	
  *!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *  SOURCE AND DESTINATION MUST BE ALIGNED !!!!
*****************************************************************/

long copy_usr2krn(int source, struct proc *src_proc, char *src_addr, char *dst_addr, int bytes)
{
	struct page *src_pg;
	void *saddr = NULL;
	unsigned long src_off;
	unsigned long dst_off;
	int 	src_npag, dst_npag;	/* number of pages the  copy implies*/
	int ret = OK;
	int len, slen, dlen;
	message *m_ptr;

	DVKDEBUG(DBGPARAMS,"source=%d src_pid=%d bytes=%d\n", 
		source, src_proc->p_usr.p_lpid, bytes);
 
	if( bytes < 0 || bytes  > MAXCOPYLEN) ERROR_RETURN(EDVSRANGE);

	src_off  = (long int) src_addr & (~PAGE_MASK);
	dst_off  = (long int) dst_addr & (~PAGE_MASK);
	DVKDEBUG(INTERNAL,"src_off=%ld dst_off=%ld\n",src_off, dst_off);
    if( src_off != dst_off ) ERROR_RETURN(EDVSALIGN);

/***************** SUPERFLOUS - CAN BE REMOVED ***********/
	src_npag = (src_off+bytes+PAGE_SIZE-1)>>PAGE_SHIFT;
	dst_npag = (dst_off+bytes+PAGE_SIZE-1)>>PAGE_SHIFT;
	DVKDEBUG(INTERNAL,"src_npag=%d dst_npag=%d\n",src_npag, dst_npag);

	while( bytes > 0) {

		down_read(&src_proc->p_task->mm->mmap_sem);
		ret = get_user_pages(src_proc->p_task, src_proc->p_task->mm,
         			(unsigned long)src_addr, 1, 1, 0, &src_pg, NULL);
		up_read(&src_proc->p_task->mm->mmap_sem);
		if (ret != 1) ERROR_RETURN(EDVSADDRNOTAVAIL);
		DVKDEBUG(INTERNAL,"get_user_pages SRC OK\n");
 
		saddr = kmap_atomic(src_pg, KM_USER0);
		DVKDEBUG(INTERNAL,"kmap_atomic SRC OK\n");

		slen = PAGE_SIZE-src_off;
		dlen = PAGE_SIZE-dst_off;
		len = min(slen,dlen);
		len = min(len,bytes);

		if( len == PAGE_SIZE) {
	    	copy_page(dst_addr , saddr);
			DVKDEBUG(INTERNAL,"copy_page %d bytes\n", len);
		}else{
	    	memcpy((dst_addr + dst_off), (saddr + src_off), len);
			DVKDEBUG(INTERNAL,"memcpy %d bytes\n", len);
		}

		/*fill the message sender field */
		if( (source != NONE) && (bytes == sizeof(message))) {
			DVKDEBUG(INTERNAL,"source=%d bytes=%d\n", source, bytes);
			m_ptr = (message *) (dst_addr + dst_off);
			m_ptr->m_source = source;
		}

		kunmap_atomic(saddr, KM_USER0);
		DVKDEBUG(INTERNAL,"kunmap_atomic SRC OK\n");
     	put_page(src_pg);
		DVKDEBUG(GENERIC,"put_page SRC OK\n");

		src_addr+=len;
		dst_addr+=len;		
		src_off  = (long int) src_addr & (~PAGE_MASK);
		dst_off  = (long int) dst_addr & (~PAGE_MASK);
		DVKDEBUG(INTERNAL,"src_off=%ld dst_off=%ld\n",src_off, dst_off);
		bytes=bytes-len;
	}

return(OK);
}

/*--------------------------------------------------------------*/
/*			kill_unbind				*/
/* src_proc->p_task is locked 					*/
/* DC is unlocked						*/
/* src_proc & dst_proc are unlocked 				*/
/*--------------------------------------------------------------*/
long kill_unbind(struct proc *dst_ptr, struct proc *src_ptr)
{
	int dst_ep, src_ep, ret = OK;
	message m, *m_ptr;

	m_ptr = &m;
	m_ptr->m_type   = MOLEXIT;
	dst_ep = dst_ptr->p_usr.p_endpoint;
	src_ep = src_ptr->p_usr.p_endpoint;
DVKDEBUG(INTERNAL,"dst_ep=%d src_ep=%d\n",dst_ep, src_ep);
//	ret = kernel_sendrec(dst_ep, m_ptr);	

	if(ret) ERROR_RETURN(ret);
	return(ret);
}


#ifdef MOLAUTOFORK
/*--------------------------------------------------------------*/
/*			fork_bind				*/
/* the parent of a process send a MOLGETPROCNR message to PM	*/
/* the PM returns the child_nr 					*/
/* the parent bind the child to the kernel			*/
/* the parent of a process send a MOLFORK message to PM		*/
/* On return, PM and SYSTASK have registered the child 		*/
/*--------------------------------------------------------------*/
struct proc* fork_bind(struct proc *proc_ptr, int child_lpid)
{
	struct proc *warn_ptr, *child_ptr;
	int src_ep, ret, dcid, warn_ep, child_ep, child_nr, child_pid;
	message m, *m_ptr;
	dc_desc_t *dc_ptr;

DVKDEBUG(DBGLVL1,"parent_ep=%d child_lpid=%d \n",proc_ptr->p_usr.p_endpoint, child_lpid);

	dcid = proc_ptr->p_usr.p_dcid;
DVKDEBUG(DBGLVL1,"dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs) 	return(NULL);
	dc_ptr 	= &dc[dcid];
	if( dc_ptr->dc_usr.dc_flags) return(NULL);

	/* Gets the endpoint of the binder   (i.e. PM) */
	warn_ep = proc_ptr->p_priv.priv_usr.priv_warn;
	if( warn_ep == NONE || warn_ep == ANY || warn_ep == SELF) 
		return(NULL); 
	warn_ptr = ENDPOINT2PTR(dc_ptr,warn_ep);
	src_ep = proc_ptr->p_usr.p_endpoint;
DVKDEBUG(DBGLVL1,"src_ep=%d warn_ep=%d \n",src_ep, warn_ep);

	/* Request to PM the next proc number */
	m_ptr = &m;
	m_ptr->m_type  = MOLFREEPROC;
	ret = kernel_sendrec(warn_ep, m_ptr);	
	if(ret) return(NULL);
	child_nr = m_ptr->PR_SLOT;
DVKDEBUG(DBGLVL1,"child_nr=%d\n",child_nr);
	if( child_nr < 0 || child_nr > dc_ptr->dc_usr.dc_nr_procs)
		return(NULL);

	child_ep = kernel_lclbind(dcid, child_lpid, child_nr);
	if(child_ep < 0) 			return(NULL);
DVKDEBUG(DBGLVL1,"child_ep=%d\n",child_ep);

	/* BIND the child into PM */
	m_ptr->m_type   = MOLFORK;
	m_ptr->PR_PID   = child_lpid;	/* LINUX PID */
	m_ptr->PR_SLOT  = child_nr;		/* PREVIOUS assigned  child_nr */		
	m_ptr->PR_ENDPT = NONE;
	ret = kernel_sendrec(warn_ep, m_ptr);	
	if(ret) 				return(NULL);
	if( m_ptr->PR_ENDPT == NONE)		return(NULL);
	child_pid = m_ptr->PR_PID;		/* MINIX PID */
	child_ep  = m_ptr->PR_ENDPT;

	child_ptr = ENDPOINT2PTR(dc_ptr,child_ep);

DVKDEBUG(DBGLVL1,"child_pid(minix)=%d child_ep=%d \n",child_pid, child_ep);

	return(child_ptr);	
}

#endif /*MOLAUTOFORK */

/****************************************************************
 *  Copy a block of data from Userspace to Userspace	
 *  Based on: https://elixir.bootlin.com/linux/v4.9.88/source/mm/process_vm_access.c
*****************************************************************/

long copy_usr2usr(int source, struct proc *src_proc, char *src_addr, struct proc *dst_proc, char *dst_addr, int bytes)
{
	struct page *src_pg;
	struct page *dst_pg;
	void *saddr = NULL;
	void *daddr = NULL;
	unsigned long src_off;
	unsigned long dst_off;
	int 	src_npag, dst_npag;	/* number of pages the  copy implies*/
	int ret = OK;
	int len, slen, dlen;
	message *m_ptr;

DVKDEBUG(DBGPARAMS,"source=%d src_pid=%d dst_pid=%d bytes=%d\n", 
		source, src_proc->p_usr.p_lpid, dst_proc->p_usr.p_lpid, bytes);
 
	if( bytes < 0 || bytes  > MAXCOPYLEN) ERROR_RETURN(EDVSRANGE);

	src_off  = (long int) src_addr & (~PAGE_MASK);
	dst_off  = (long int) dst_addr & (~PAGE_MASK);
DVKDEBUG(INTERNAL,"src_off=%ld dst_off=%ld\n",src_off, dst_off);

/***************** SUPERFLOUS - CAN BE REMOVED ***********/
	src_npag = (src_off+bytes+PAGE_SIZE-1)>>PAGE_SHIFT;
	dst_npag = (dst_off+bytes+PAGE_SIZE-1)>>PAGE_SHIFT;
DVKDEBUG(INTERNAL,"src_npag=%d dst_npag=%d\n",src_npag, dst_npag);

	while( bytes > 0) {

		down_read(&src_proc->p_task->mm->mmap_sem);
		ret = get_user_pages(src_proc->p_task, src_proc->p_task->mm,
         			(unsigned long)src_addr, 1, 1, 0, &src_pg, NULL);
		up_read(&src_proc->p_task->mm->mmap_sem);
		if (ret != 1) {
			ERROR_RETURN(EDVSADDRNOTAVAIL);
		}
DVKDEBUG(INTERNAL,"get_user_pages SRC OK\n");
 
		down_read(&dst_proc->p_task->mm->mmap_sem);
		ret = get_user_pages(dst_proc->p_task, dst_proc->p_task->mm,
     	         	(unsigned long)dst_addr, 1, 1, 0, &dst_pg, NULL);
		up_read(&dst_proc->p_task->mm->mmap_sem);
		if (ret != 1) {
	     	put_page(src_pg);
			ERROR_RETURN(EDVSADDRNOTAVAIL);
		}
DVKDEBUG(INTERNAL,"get_user_pages DST OK\n");

		saddr = kmap_atomic(src_pg, KM_USER0);
DVKDEBUG(INTERNAL,"kmap_atomic SRC OK\n");
		daddr = kmap_atomic(dst_pg, KM_USER0);
DVKDEBUG(INTERNAL,"kmap_atomic DST OK\n");

		slen = PAGE_SIZE-src_off;
		dlen = PAGE_SIZE-dst_off;
		len = min(slen,dlen);
		len = min(len,bytes);

		if( len == PAGE_SIZE) {
	    		copy_page(daddr , saddr);
DVKDEBUG(INTERNAL,"copy_page %d bytes\n", len);

		}else{
	    		memcpy((daddr + dst_off), (saddr + src_off), len);
DVKDEBUG(INTERNAL,"memcpy %d bytes\n", len);
		}

		/*fill the message sender field */
		if( (source != NONE) && (bytes == sizeof(message))) {
DVKDEBUG(INTERNAL,"source=%d bytes=%d\n", source, bytes);
			m_ptr = (message *) (daddr + dst_off);
			m_ptr->m_source = source;
		}

		kunmap_atomic(saddr, KM_USER0);
DVKDEBUG(INTERNAL,"kunmap_atomic SRC OK\n");
		kunmap_atomic(daddr, KM_USER0);
DVKDEBUG(INTERNAL,"kunmap_atomic DST OK\n");
		set_page_dirty_lock(dst_pg);
DVKDEBUG(INTERNAL,"set_page_dirty_lock OK\n");
     	put_page(dst_pg);
DVKDEBUG(GENERIC,"put_page DST OK\n");
     	put_page(src_pg);
DVKDEBUG(GENERIC,"put_page SRC OK\n");

		src_addr+=len;
		dst_addr+=len;		
		src_off  = (long int) src_addr & (~PAGE_MASK);
		dst_off  = (long int) dst_addr & (~PAGE_MASK);
DVKDEBUG(INTERNAL,"src_off=%ld dst_off=%ld\n",src_off, dst_off);
		bytes=bytes-len;
	}

return(OK);
}

#endif // NULL_CODE


