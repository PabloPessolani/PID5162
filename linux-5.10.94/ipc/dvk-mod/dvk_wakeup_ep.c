/*--------------------------------------------------------------*/
/*			mm_wakeup_ep				*/
/*--------------------------------------------------------------*/
asmlinkage long mm_wakeup_ep(int dcid, int proc_ep, int signo)
{

	struct proc *proc_ptr, *other_ptr, *sproxy_ptr;
	dc_desc_t *dc_ptr;
	int caller_nr, caller_pid, caller_ep;
	int dst_nr, dcid;
	int ret;
	struct task_struct *task_ptr;	
	struct timespec *t_ptr;
	message *mptr;

	DVKDEBUG(DBGPARAMS,"dcid=%d proc_ep=%d\n", dcid, proc_ep);
	if( DVS_NOT_INIT() ) 			
		ERROR_RETURN(EDVSDVSINIT);
#ifdef DVS_CAPABILITIES
	 if ( !capable(CAP_VOS_ADMIN)) ERROR_RETURN(EDVSPRIVILEGES);
#else
	if(get_current_cred()->euid.val != USER_ROOT) ERROR_RETURN(EDVSPRIVILEGES);
#endif 
		
	task_ptr = current;
	caller_pid  = task_pid_nr(current);
	caller_tgid = current->tgid;
	DVKDEBUG(DBGPARAMS,"caller_pid=%d caller_tgid=%d\n", caller_pid, caller_tgid);
	
	if( dcid < 0 || dcid >= dvs.d_nr_dcs) 			
		ERROR_RETURN(EDVSBADDCID);
	dc_ptr 	= &dc[dcid];
	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags)  
		ERROR_RUNLOCK_DC(dc_ptr,EDVSDCNOTRUN);
	RUNLOCK_DC(dc_ptr);	

	/*------------------------------------------
	 * get the destination process number
	*------------------------------------------*/
	dst_nr = _ENDPOINT_P(proc_ep);
	if( dst_nr < (-dc_ptr->dc_usr.dc_nr_tasks)		
	 || dst_nr >= dc_ptr->dc_usr.dc_nr_procs)	
		ERROR_RETURN(EDVSRANGE)
	proc_ptr   = NBR2PTR(dc_ptr, dst_nr);
	if( proc_ptr == NULL) 				
		ERROR_RETURN(EDVSDSTDIED);
	if( caller_ep == proc_ep) 			
		ERROR_RETURN(EDVSENDPOINT);
	if( current == proc_ptr->p_task)
		ERROR_RETURN(EDVSBADPROC);
	
	/*------------------------------------------
	 * check the destination process status
	*------------------------------------------*/
	WLOCK_PROC(proc_ptr);
	DVKDEBUG(DBGPARAMS,"dst_nr=%d proc_ep=%d\n",dst_nr, proc_ptr->p_usr.p_endpoint);
	if (proc_ptr->p_usr.p_endpoint != proc_ep) 	{
		WUNLOCK_PROC(proc_ptr);
		ERROR_RETURN(EDVSENDPOINT);
	}
	if( test_bit(BIT_SLOT_FREE, &proc_ptr->p_usr.p_rts_flags)){
		WUNLOCK_PROC(proc_ptr);
		ERROR_RETURN(EDVSDSTDIED);
	}
	if( proc_ptr->p_usr.p_nodeid < 0 
		|| proc_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 	 {
		WUNLOCK_PROC(proc_ptr);
		ERROR_RETURN(EDVSBADNODEID);
	} 
	if( ! test_bit(proc_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)){ 	
		WUNLOCK_PROC(proc_ptr);
		ERROR_RETURN(EDVSNODCNODE);
	}
	if( test_bit(BIT_MIGRATE, &proc_ptr->p_usr.p_rts_flags)) {	/*destination is migrating	*/
		DVKDEBUG(GENERIC,"destination is migrating proc_ptr->p_usr.p_rts_flags=%lX\n"
			,proc_ptr->p_usr.p_rts_flags);
		WUNLOCK_PROC(proc_ptr);
		return(EDVSMIGRATE);
	}

	if( IT_IS_REMOTE(proc_ptr)) {	
		WUNLOCK_PROC(proc_ptr);
		return(EDVSRMTPROC);
	} 

	/*---------------------------------------------------------------------------------------------------*/
	/*						LOCAL  								 */
	/*---------------------------------------------------------------------------------------------------*/
	/* Check if 'dst' is blocked waiting something.   */
	if (  proc_ptr->p_usr.p_rts_flags != 0 ) {

		if (test_bit(BIT_SENDING, &proc_ptr->p_usr.p_rts_flags)){
			if( ! test_bit(BIT_RMTOPER, &proc_ptr->p_usr.p_rts_flags)){
//				rp = DC_PROC(dc_ptr,(_ENDPOINT_P(proc_ptr->p_usr.p_sendto) + dc_ptr->dc_usr.dc_nr_tasks));
				rp = ENDPOINT2PTR(dc_ptr, proc_ptr->p_usr.p_sendto);
				uproc_ptr = &rp->p_usr;
				DVKDEBUG(INTERNAL,PROC_USR_FORMAT,PROC_USR_FIELDS(uproc_ptr));
				if( proc_ptr->p_usr.p_nr < rp->p_usr.p_nr) {
					WLOCK_PROC(rp); /* Caller LOCK is just locked */
				}else{	
					/* free the callers lock and then lock both ordered */
					WUNLOCK_PROC(proc_ptr);
					WLOCK_PROC(rp);
					WLOCK_PROC(proc_ptr);
				}
				LIST_DEL_INIT(&proc_ptr->p_link); /* remove from queue */
				WUNLOCK_PROC(rp);	
			}
			proc_ptr->p_usr.p_sendto	= NONE;
			clear_bit(BIT_SENDING, &proc_ptr->p_usr.p_rts_flags);
		}
				
		if( test_bit(BIT_RMTOPER, &proc_ptr->p_usr.p_rts_flags)){
			if(proc_ptr->p_usr.p_proxy != NONE) {
				sproxy_ptr = &proxies[proc_ptr->p_usr.p_proxy].px_sproxy;
				WLOCK_PROC(sproxy_ptr);
				LIST_DEL_INIT(&proc_ptr->p_link); /* remove from queue */
				WUNLOCK_PROC(sproxy_ptr);
			}else{
				ERROR_WUNLOCK_PROC(proc_ptr,EDVSPROCSTS);
			}
			clear_bit(BIT_RMTOPER, &proc_ptr->p_usr.p_rts_flags);
			proc_ptr->p_usr.p_proxy = NONE;
		}	

		if( test_bit(BIT_RECEIVING, &proc_ptr->p_usr.p_rts_flags)){
			clear_bit(BIT_RECEIVING, &proc_ptr->p_usr.p_rts_flags);
			proc_ptr->p_usr.p_getfrom	= NONE;
		}
		
		if(proc_ptr->p_usr.p_rts_flags == 0){
			LOCAL_PROC_UP(proc_ptr, EDVSWOKENUP); 
		}else{
			DVKDEBUG(GENERIC,"Process status is p_rts_flags=%lX\n",
				proc_ptr->p_usr.p_rts_flags);				
			set_bit(MIS_BIT_WOKENUP, &proc_ptr->p_usr.p_misc_flags))	
			ERROR_WUNLOCK_PROC(proc_ptr,EDVSPROCSTS);
		}
	} else { 
		DVKDEBUG(GENERIC,"Process is running p_rts_flags=%lX\n",
			proc_ptr->p_usr.p_rts_flags);
		set_bit(MIS_BIT_WOKENUP, &proc_ptr->p_usr.p_misc_flags))	
		ERROR_WUNLOCK_PROC(proc_ptr,EDVSPROCRUN);
	}
	WUNLOCK_PROC(proc_ptr);

	return(OK);
}
