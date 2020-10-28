/****************************************************************/
/*		MINIX OVER LINUX IPC PRIMITIVES FOR PROXIES	*/
/****************************************************************/

//asmlinkage long mm_bind(int dcid, int pid, int proc, int nodeid);

#include "dvk_mod.h"

/****************************************************************/
/****************************************************************/
/*	FUNCTIONS FOR SENDER PROXY				*/
/****************************************************************/
/****************************************************************/

/*--------------------------------------------------------------*/
/*			sproxy_enqueue				*/
/* Enqueue into the sender's proxy queue  the proc_ptr   	*/
/* descriptor with the requester rqtr and remote        	*/
/* operation rmtoper 						*/
/*--------------------------------------------------------------*/
long sproxy_enqueue(struct proc *proc_ptr)
{
	struct proc *sproxy_ptr;
	proc_usr_t *p_ptr;
	cmd_t *c_ptr;
 
	p_ptr = &proc_ptr->p_usr;
	DVKDEBUG(DBGPARAMS, PROC_USR_FORMAT, PROC_USR_FIELDS(p_ptr));
	c_ptr = &proc_ptr->p_rmtcmd;
	DVKDEBUG(DBGPARAMS, CMD_FORMAT, CMD_FIELDS(c_ptr));
	
	sproxy_ptr =  NODE2SPROXY(proc_ptr->p_rmtcmd.c_dnode);
	WLOCK_PROC(sproxy_ptr);
	if( sproxy_ptr->p_usr.p_rts_flags == SLOT_FREE) ERROR_WUNLOCK_PROC(sproxy_ptr, EDVSNOPROXY);

	/* save the proxy pair number to simplify garbage collection */
	proc_ptr->p_usr.p_proxy = sproxy_ptr->p_usr.p_nr;

	/* Check if the descriptor is already enqueued */
	if( test_bit(BIT_RMTOPER, &proc_ptr->p_usr.p_rts_flags)) {
		DVKDEBUG(INTERNAL, "QUEUING ERROR " PROC_USR_FORMAT, PROC_USR_FIELDS(p_ptr));
	}    
	/* set the RMTOPER bit to signal that this REMOTE descriptor 	*/
	/* is envolved into a remote operation 			*/	
	set_bit(BIT_RMTOPER, &proc_ptr->p_usr.p_rts_flags);

	INIT_LIST_HEAD(&proc_ptr->p_link);
	LIST_ADD_TAIL(&proc_ptr->p_link , &sproxy_ptr->p_list);
	p_ptr = &sproxy_ptr->p_usr;
	DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(p_ptr));
	if( test_bit(BIT_RECEIVING, &sproxy_ptr->p_usr.p_rts_flags))  {
		clear_bit(BIT_RECEIVING,&sproxy_ptr->p_usr.p_rts_flags);
		if(sproxy_ptr->p_usr.p_rts_flags == 0) {
			LOCAL_PROC_UP(sproxy_ptr, OK);
		}
	}
	WUNLOCK_PROC(sproxy_ptr);	
	return(OK);
}

//################################################################
//################################################################
//####################### AQUI ####################################
//################################################################
//################################################################
//################################################################
//################################################################

/*--------------------------------------------------------------*/
/*			new_get2rmt				*/
/* proxy gets local (messages, notifies, errors, ups, data,etc  */
/* to send to a remote processes	 			*/
/* usr_hdr_ptr: buffer address in userspace for the header	*/
/* usr_pay_ptr: buffer address in userspace for the payload	*/
/*--------------------------------------------------------------*/
asmlinkage long new_get2rmt(proxy_hdr_t *usr_hdr_ptr, proxy_payload_t *usr_pay_ptr, long timeout_ms)
{
	dc_desc_t *dc_ptr;
	struct proc *src_ptr, *dst_ptr;
	struct proc *src_vc_ptr, *dst_vc_ptr;
	struct proc	*xpp, *tmp_ptr,*sproxy_ptr;
	proxy_hdr_t *h_ptr;
	struct task_struct *task_ptr;	
	int ret, dcid;
	int sproxy_pid, px_nr;
	cluster_node_t *node_ptr;
	node_usr_t *nu_ptr;
	struct timespec *t_ptr;
	proc_usr_t *p_ptr;
	proxies_t *px_ptr;
	cmd_t *c_ptr;
	message *m_ptr;
	int src_nr, dst_nr, xpp_nr;

	DVKDEBUG(GENERIC,"\n");

#ifdef DVS_CAPABILITIES
	 if ( !capable(CAP_DVS_ADMIN)) ERROR_RETURN(EDVSPRIVILEGES);
#else
	if(get_current_cred()->euid.val != USER_ROOT) ERROR_RETURN(EDVSPRIVILEGES);
#endif 

	if( DVS_NOT_INIT() )   return(EDVSDVSINIT );
	
	ret = check_caller(&task_ptr, &sproxy_ptr, &sproxy_pid);
	if(ret) 	return(ret);

	/*------------------------------------------
	 * Check the status of the proxy (caller)
         *------------------------------------------*/
	ret = OK;
	WLOCK_PROC(sproxy_ptr);
	do {
		if( ! get_sys_bit(sproxy_ptr->p_priv.priv_usr.priv_dvk_allowed, DVK_GET2RMT ))
			{ret = EDVSBADCALL;break;} 
		if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	{ret = EDVSNOPROXY;break;} 
		if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags)){ret = EDVSNOTCONN;break;}  
	} while(0);
	if(ret) {
		WUNLOCK_PROC(sproxy_ptr);
		ERROR_RETURN(ret);
	}

	/*------------------------------------------
	 * Verify if SPROXY is correctectly set on proxies struct
         *------------------------------------------*/
	px_nr = sproxy_ptr->p_usr.p_nr;
	px_ptr = &proxies[px_nr];
	sproxy_ptr->p_umsg = (message *) usr_pay_ptr;
	/* required by locking order */
	WUNLOCK_PROC(sproxy_ptr);
	RLOCK_PROXY(px_ptr);
	if( sproxy_ptr != &px_ptr->px_sproxy) {
		RUNLOCK_PROXY(px_ptr);
		ERROR_RETURN(EDVSBADPROXY);
	}
	RUNLOCK_PROXY(px_ptr);

	/*------------------------------------------
	 * Get the command from the proxy queue
         *------------------------------------------*/
	RLOCK_PROC(sproxy_ptr);
	while( TRUE )	{
		DVKDEBUG(GENERIC,"LIST_FOR_EACH_ENTRY_SAFE\n");
		ret = OK;
		LIST_FOR_EACH_ENTRY_SAFE(xpp, tmp_ptr, &sproxy_ptr->p_list, p_link) {

			RUNLOCK_PROC(sproxy_ptr);
			WLOCK_PROC(xpp);
			WLOCK_PROC(sproxy_ptr);
			xpp_nr = xpp->p_usr.p_nr;

			DVKDEBUG(GENERIC,"Found a message. p_endpoint=%d c_cmd=%d\n",
				xpp->p_usr.p_endpoint,xpp->p_rmtcmd.c_cmd);
			
			LIST_DEL_INIT(&xpp->p_link); /* remove from queue */
			
			/* A LOCAL process descriptor must have	(SENDING | ONCOPY) set	*/
			/* A REMOTE process descriptor must have (RMTOPER | ONCOPY) set	*/
			/* Those flags protect them when unlocks			*/ 
			/* RULE TO LOCK: 1st: sender, 2nd: sender proxy */

			do {
				ret = OK;
				
				p_ptr = &xpp->p_usr;
				DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(p_ptr));

				/*-----------------------------------
				 * Check process descriptor status
				 *----------------------------------*/	
				c_ptr = &xpp->p_rmtcmd;
				DVKDEBUG(INTERNAL, CMD_FORMAT, CMD_FIELDS(c_ptr));

				m_ptr = &xpp->p_message;
				DVKDEBUG(INTERNAL, MSG1_FORMAT, MSG1_FIELDS(m_ptr));		
			
				node_ptr = &node[xpp->p_rmtcmd.c_dnode];
				nu_ptr = &node_ptr->n_usr;
				DVKDEBUG(INTERNAL, NODE_USR_FORMAT, NODE_USR_FIELDS(nu_ptr));

				if( test_bit(BIT_SLOT_FREE, &xpp->p_usr.p_rts_flags)) 	{ret = EDVSNOTBIND; break;}
				if(!test_bit(xpp->p_usr.p_dcid,&node_ptr->n_usr.n_dcs)) {ret = EDVSNODCNODE;break;}
				if( (xpp->p_usr.p_dcid < 0 )
				 || (xpp->p_usr.p_dcid >= dvs_ptr->d_nr_dcs))			{ret = EDVSBADDCID;break;} 	
				if( test_bit(BIT_MIGRATE, &xpp->p_usr.p_rts_flags)) 	{ret = EDVSMIGRATE; break;}
				if(!test_bit(xpp->p_rmtcmd.c_dnode,&sproxy_ptr->p_usr.p_nodemap)) {ret = EDVSNONODE;break;}
			}while(0);
			WUNLOCK_PROC(sproxy_ptr);
			dc_ptr 	= &dc[xpp->p_usr.p_dcid];
			WUNLOCK_PROC(xpp);	

			RLOCK_DC(dc_ptr);
			if( !ret) {	
				/*-----------------------------------
				 * Check DC status
				 *----------------------------------*/	
				if(dc_ptr->dc_usr.dc_flags) {
					ret = EDVSDCNOTRUN; 
				}else{
					dcid = dc_ptr->dc_usr.dc_dcid;
					WLOCK_PROC(sproxy_ptr);
					/* The proxy takes the sender's DCID personality */
					sproxy_ptr->p_usr.p_dcid = dcid;
					WUNLOCK_PROC(sproxy_ptr);
//					DC_INCREF(dc_ptr);
				}
			} 
			dst_ptr 	= ENDPOINT2PTR(dc_ptr, xpp->p_rmtcmd.c_dst);
			src_vc_ptr = ENDPOINT2PTR(dc_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_src);				
			dst_vc_ptr = ENDPOINT2PTR(dc_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_dst);
			RUNLOCK_DC(dc_ptr);

			WLOCK_PROC(xpp);	
			/* do not replace by "else" !! */
			if(ret) {
				/*-----------------------------------
				 * For Requests (LOCAL descriptors) 
				 * return an ACK with error
				 *----------------------------------*/
				xpp->p_usr.p_proxy = NONE;	
				if(IT_IS_LOCAL(xpp)){
					xpp->p_usr.p_rts_flags = 0;
					READY_UP_RCODE(xpp, MASK_ACKNOWLEDGE, ret);
					WUNLOCK_PROC(xpp);	
					ERROR_RETURN(ret);
				}
				/*-----------------------------------
				 * For ACKs (REMOTE descriptors) 
				 * IGNORE
				 *----------------------------------*/
				xpp->p_usr.p_rts_flags = 0;
				set_bit(BIT_REMOTE, &xpp->p_usr.p_rts_flags);
				WUNLOCK_PROC(xpp);	
				RLOCK_PROC(sproxy_ptr);
				ERROR_PRINT(ret);
				continue; /* get next message  */
			}

			h_ptr = &xpp->p_rmtcmd;
			ret = OK;

			/*
			* Verify MIGRATED destination PROCESS
			* If command is SEND, SENDREC, NOTIFY, REPLY replay this commands for the local process
			* Any ONCOPY command must be in queue because the migration checks the ONCOPY bit.
			* Any ACK command must be in queue because the MIGRATING process is blocked on a SENDREC to its SYSTASK
			*/
			if( IT_IS_LOCAL(xpp) ) {
				dst_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_dst);
				WUNLOCK_PROC(xpp);					
				WLOCK_ORDERED2(xpp_nr, dst_nr, xpp, dst_ptr);							
				if( h_ptr->c_dnode != dst_ptr->p_usr.p_nodeid ){ /* It means that the dest process has migrated */
					WUNLOCK_PROC(dst_ptr);		
					/*
					* Wakes up the local sender proc with an EDVSMIGRATE error to REPLAY the IPC.
					*/
					xpp->p_usr.p_sendto = NONE;
					xpp->p_usr.p_proxy 	= NONE;	
					xpp->p_usr.p_rts_flags = 0;
					READY_UP_RCODE(xpp, MASK_ACKNOWLEDGE, EDVSMIGRATE)
					WUNLOCK_PROC(xpp);
					RLOCK_PROC(sproxy_ptr); /* the proxy must be locked for the next loop 	*/
					continue;		/* next loop of LIST_FOR_EACH_ENTRY_SAFE 	*/	
				}else{
					WUNLOCK_PROC(dst_ptr);	
				}
			} 

			h_ptr->c_timestamp = current_kernel_time();
			t_ptr = &h_ptr->c_timestamp;
			DVKDEBUG(INTERNAL,TIME_FORMAT, TIME_FIELDS(t_ptr));

			if(IT_IS_REMOTE(xpp)){ 	/* REMOTE process descriptor used for ACKNOWLEDGES 	*/
				DVKDEBUG(DBGPROC,"REMOTE " PROC_USR_FORMAT,PROC_USR_FIELDS(p_ptr));
				switch(xpp->p_rmtcmd.c_cmd) {
					case CMD_SEND_ACK:	/* The local receiver process send a SEND ACK to the remote sender process */
						DVKDEBUG(GENERIC,"CMD_SEND_ACK\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						break;
					case CMD_SNDREC_ACK:	/* The local receiver process send a SENDREC ACK to the remote sender process  ONLY ON ERROR */
						DVKDEBUG(GENERIC,"CMD_SNDREC_ACK\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						break;
#ifdef RMT_PROCINFO					
					case CMD_PROCINFO_ACK:	
						DVKDEBUG(GENERIC,"CMD_PROCINFO_ACK\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));						
						break;
#endif //  RMT_PROCINFO
						
					case CMD_COPYIN_ACK:	
						DVKDEBUG(GENERIC,"CMD_COPYIN_ACK\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						/* Other  process could be in the same slot  */
						/*!! the source of the ACK was the destination of the COPY !!*/
						dst_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_dst);
						WUNLOCK_PROC(xpp);
						WLOCK_ORDERED2(xpp_nr, src_nr, xpp, dst_vc_ptr);							
						if(dst_vc_ptr->p_usr.p_endpoint == xpp->p_rmtcmd.c_src) {
							clear_bit(BIT_ONCOPY, &dst_vc_ptr->p_usr.p_rts_flags);
						}else{
							ret = EDVSBADPROC;
						}
						WUNLOCK_PROC(dst_vc_ptr);
						break;
					case CMD_COPYOUT_DATA:	/* the remote process has requested to send local data */
						DVKDEBUG(GENERIC,"CMD_COPYOUT_DATA\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						/*  Copy the payload from source's user space to proxy's user space */
						src_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_src);
						WUNLOCK_PROC(xpp);
						WLOCK_ORDERED2(xpp_nr, src_nr, xpp, src_vc_ptr);
						/* Other  process could be in the same slot  */
						/*!! the source of the DATA is the source of the COPY !!*/
						if(src_vc_ptr->p_usr.p_endpoint == xpp->p_rmtcmd.c_src) {
							if(xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes > NODE2MAXBYTES(xpp->p_rmtcmd.c_dnode))
								ret = EDVS2BIG;
							if( xpp->p_rmtcmd.c_rcode == OK){
								RLOCK_PROC(sproxy_ptr);
								if( test_bit(MIS_BIT_KTHREAD, &sproxy_ptr->p_usr.p_misc_flags))	{
									COPY_USR2KRN(ret, NONE, src_vc_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_saddr,
										(char*) usr_pay_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes);
								}else{
									COPY_USR2USR_PROC(ret, NONE, src_vc_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_saddr, 
										sproxy_ptr, (char*) usr_pay_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes);
								}
								RUNLOCK_PROC(sproxy_ptr);							
							}	
							clear_bit(BIT_ONCOPY, &src_vc_ptr->p_usr.p_rts_flags);
						}else{
							ret = EDVSBADPROC;
						}
						WUNLOCK_PROC(src_vc_ptr);
						break;
					case CMD_COPYIN_RQST:
						DVKDEBUG(GENERIC,"CMD_COPYIN_RQST\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						/*  Copy the payload from source's user space to proxy's user space */
//						dc_ptr 	= &dc[xpp->p_usr.p_dcid];
						src_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_src);
						WUNLOCK_PROC(xpp);			
						WLOCK_ORDERED2(xpp_nr, src_nr, xpp, src_vc_ptr);	
						/* Other  process could be in the same slot  */
						/*!! the source of the RQST is the source of the COPY !!*/
						if(src_vc_ptr->p_usr.p_endpoint == xpp->p_rmtcmd.c_src) {
							if(xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes > NODE2MAXBYTES(xpp->p_rmtcmd.c_dnode))
								ret = EDVS2BIG;
							if( xpp->p_rmtcmd.c_rcode == OK){
								RLOCK_PROC(sproxy_ptr);
								if( test_bit(MIS_BIT_KTHREAD, &sproxy_ptr->p_usr.p_misc_flags))	{
									COPY_USR2KRN(ret, NONE, src_vc_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_saddr,
											(char*) usr_pay_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes);
								}else{
									COPY_USR2USR_PROC(ret, NONE, src_vc_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_saddr, 
											sproxy_ptr, (char*) usr_pay_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes);
								RUNLOCK_PROC(sproxy_ptr);
								}
							}
							clear_bit(BIT_ONCOPY, &src_vc_ptr->p_usr.p_rts_flags);
							clear_bit(BIT_ONCOPY, &xpp->p_usr.p_rts_flags);
						}else{
							ret = EDVSBADPROC;
						}
						WUNLOCK_PROC(src_vc_ptr);
						break;
					case CMD_COPYLCL_ACK:	
						DVKDEBUG(GENERIC,"CMD_COPYLCL_ACK\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
//						dc_ptr 	= &dc[xpp->p_usr.p_dcid];
						src_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_src);
						WUNLOCK_PROC(xpp);
						WLOCK_ORDERED2(xpp_nr, src_nr, xpp, src_vc_ptr);						
						/* Other  process could be in the same slot  */
						/* !!! The source of the ACK was the source of the copy  !!!*/
						if( src_vc_ptr->p_usr.p_endpoint == xpp->p_rmtcmd.c_src) {
							clear_bit(BIT_ONCOPY, &src_vc_ptr->p_usr.p_rts_flags);
							clear_bit(BIT_ONCOPY, &xpp->p_usr.p_rts_flags);
						}else{
							ret = EDVSBADPROC;
						}
						WUNLOCK_PROC(src_vc_ptr);
						break;
					case CMD_COPYRMT_ACK:	
						DVKDEBUG(GENERIC,"CMD_COPYRMT_ACK\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
//						dc_ptr 	= &dc[xpp->p_usr.p_dcid];

						src_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_src);
						dst_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_src);

						WUNLOCK_PROC(xpp);
						WLOCK_ORDERED3(xpp_nr, src_nr, dst_nr, xpp, src_vc_ptr, dst_vc_ptr);	
						/* Other  process could be in the same slot  */
						/*!!! the source of the ACK was the destination of the copy !!!*/
						if( (dst_vc_ptr->p_usr.p_endpoint == xpp->p_rmtcmd.c_src) 
						 && (xpp->p_usr.p_endpoint == xpp->p_rmtcmd.c_u.cu_vcopy.v_rqtr)){		
							clear_bit(BIT_ONCOPY, &src_vc_ptr->p_usr.p_rts_flags);
							clear_bit(BIT_ONCOPY, &dst_vc_ptr->p_usr.p_rts_flags);
							clear_bit(BIT_ONCOPY, &xpp->p_usr.p_rts_flags);
						}else{
							ret = EDVSBADPROC;
						}			
						WUNLOCK_PROC2(src_vc_ptr, dst_vc_ptr);
						break;			
					default:
						DVKDEBUG(GENERIC,"BAD CMD=%d\n",xpp->p_rmtcmd.c_cmd);
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						ret = EDVSINVAL;
						break;
				}
				if(ret < 0) {
					xpp->p_usr.p_rts_flags = 0;
					set_bit(BIT_REMOTE, &xpp->p_usr.p_rts_flags);
					WUNLOCK_PROC(xpp);

					WLOCK_PROC(sproxy_ptr);
					sproxy_ptr->p_usr.p_dcid = (-1);
					WUNLOCK_PROC(sproxy_ptr);

//					DC_DECREF(dc_ptr);
					ERROR_RETURN(ret);
				}
				clear_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags);
				clear_bit(BIT_RMTOPER, &xpp->p_usr.p_rts_flags);
				xpp->p_usr.p_sendto	= NONE;
			}else { 	/* the LOCAL process need to send a MINIX message to remote  */
				DVKDEBUG(DBGPROC,"LOCAL " PROC_USR_FORMAT,PROC_USR_FIELDS(p_ptr));
				clear_bit(BIT_RMTOPER, &xpp->p_usr.p_rts_flags);
				switch(xpp->p_rmtcmd.c_cmd) {
					case CMD_COPYIN_DATA:
						DVKDEBUG(GENERIC,"CMD_COPYIN_DATA\n");
						DVKDEBUG(DBGVCOPY,CMD_FORMAT,CMD_FIELDS(h_ptr));
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						/*  Copy the payload from source's user space to proxy's user space */
						/*!! the source of the REQUEST is the source of the COPY !!*/				
						if(src_vc_ptr != xpp) { 
							src_nr = _ENDPOINT_P(xpp->p_rmtcmd.c_u.cu_vcopy.v_src);
							WUNLOCK_PROC(xpp);	
							WLOCK_ORDERED2(xpp_nr, src_nr, xpp, src_vc_ptr);
							if(xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes > NODE2MAXBYTES(xpp->p_rmtcmd.c_dnode))
								ret = EDVS2BIG;
							if (ret == OK) {
								RLOCK_PROC(sproxy_ptr);
								if( test_bit(MIS_BIT_KTHREAD, &sproxy_ptr->p_usr.p_misc_flags))	{
									COPY_USR2KRN(ret, NONE, src_vc_ptr, h_ptr->c_u.cu_vcopy.v_saddr,
											(char*) usr_pay_ptr, xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes);
								}else{
									COPY_USR2USR_PROC(ret, NONE, src_vc_ptr, h_ptr->c_u.cu_vcopy.v_saddr, 
										sproxy_ptr, (char*) usr_pay_ptr, h_ptr->c_u.cu_vcopy.v_bytes);
								}
								RUNLOCK_PROC(sproxy_ptr);
							}
							WUNLOCK_PROC(src_vc_ptr);
						} else {
							if(xpp->p_rmtcmd.c_u.cu_vcopy.v_bytes > NODE2MAXBYTES(xpp->p_rmtcmd.c_dnode))
								ret = EDVS2BIG;
							if (ret == OK) {
								RLOCK_PROC(sproxy_ptr);
								if( test_bit(MIS_BIT_KTHREAD, &sproxy_ptr->p_usr.p_misc_flags))	{
									COPY_USR2KRN(ret, NONE, src_vc_ptr, h_ptr->c_u.cu_vcopy.v_saddr,
											(char*) usr_pay_ptr, h_ptr->c_u.cu_vcopy.v_bytes);
								}else{							
									COPY_USR2USR_PROC(ret, NONE, src_vc_ptr, h_ptr->c_u.cu_vcopy.v_saddr, 
										sproxy_ptr, (char*) usr_pay_ptr, h_ptr->c_u.cu_vcopy.v_bytes);
								}
								RUNLOCK_PROC(sproxy_ptr);
							}
						}
						break;
					case CMD_COPYOUT_RQST:
						DVKDEBUG(GENERIC,"CMD_COPYOUT_RQST\n");
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						break;		
					case CMD_COPYLCL_RQST:
						DVKDEBUG(GENERIC,"CMD_COPYLCL_RQST\n");
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						break;
					case CMD_COPYRMT_RQST:
						DVKDEBUG(GENERIC,"CMD_COPYRMT_RQST\n");
						DVKDEBUG(DBGVCOPY,VCOPY_FORMAT,VCOPY_FIELDS(h_ptr));
						break;
					case CMD_SNDREC_MSG:
						DVKDEBUG(GENERIC,"CMD_SNDREC_MSG\n");
						xpp->p_usr.p_sendto = NONE;
						clear_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags);
					case CMD_SEND_MSG:
						if (xpp->p_rmtcmd.c_cmd == CMD_SEND_MSG) 
							DVKDEBUG(GENERIC,"CMD_SEND_MSG\n");				
						m_ptr = &h_ptr->c_u.cu_msg;
						m_ptr->m_source = xpp->p_usr.p_endpoint;
						DVKDEBUG(GENERIC, MSG1_FORMAT, MSG1_FIELDS(m_ptr));
						break;
#ifdef RMT_PROCINFO					
					case CMD_PROCINFO:
						if (xpp->p_rmtcmd.c_cmd == CMD_PROCINFO) 
							DVKDEBUG(GENERIC,"CMD_PROCINFO\n");				
						clear_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags);
						m_ptr = &h_ptr->c_u.cu_msg;
						break;
#endif // RMT_PROCINFO

					case CMD_REPLY_MSG:
						DVKDEBUG(GENERIC,"CMD_REPLY_MSG\n");
						m_ptr = &h_ptr->c_u.cu_msg;
						m_ptr->m_source = xpp->p_usr.p_endpoint;
						DVKDEBUG(GENERIC, MSG1_FORMAT, MSG1_FIELDS(m_ptr));
						break;
					case CMD_NTFY_MSG:
						DVKDEBUG(GENERIC,"CMD_NTFY_MSG\n");
						xpp->p_usr.p_sendto 	= NONE;
						m_ptr = &h_ptr->c_u.cu_msg;
						m_ptr->m_source = xpp->p_usr.p_endpoint;
						DVKDEBUG(GENERIC, MSG9_FORMAT, MSG9_FIELDS(m_ptr));
						clear_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags);
						clear_bit(BIT_RMTOPER, &xpp->p_usr.p_rts_flags);
						if(xpp->p_usr.p_rts_flags == 0) 			
							LOCAL_PROC_UP(xpp, ret);
						break;
					default:
						ret = EDVSINVAL;
						break;
				}
				xpp->p_usr.p_proxy = NONE;
				if(ret < 0) {
					xpp->p_usr.p_rts_flags = 0;
					READY_UP_RCODE(xpp, MASK_ACKNOWLEDGE, ret);
					WUNLOCK_PROC(xpp);
//					DC_DECREF(dc_ptr);

					WLOCK_PROC(sproxy_ptr);
					sproxy_ptr->p_usr.p_dcid = (-1);
					WUNLOCK_PROC(sproxy_ptr);

					ERROR_RETURN(ret);
				}

			}

			h_ptr->c_flags    = 0;
			h_ptr->c_snd_seq  = 0;
			h_ptr->c_ack_seq  = 0;
			h_ptr->c_batch_nr = 0;			
			DVKDEBUG(DBGCMD,HDR_FORMAT,HDR_FIELDS(h_ptr));
			DVKDEBUG(DBGCMD,CMD_XFORMAT,CMD_XFIELDS(h_ptr));
			/*  Copy the header to proxy */
			WLOCK_PROC(sproxy_ptr);
			if( test_bit(MIS_BIT_KTHREAD, &sproxy_ptr->p_usr.p_misc_flags))	{
				memcpy(usr_hdr_ptr, h_ptr, sizeof(proxy_hdr_t));
			}else{							
				COPY_TO_USER_PROC(ret, h_ptr, usr_hdr_ptr, sizeof(proxy_hdr_t));
			}
			xpp->p_rmtcmd.c_cmd = CMD_NONE;
			xpp->p_usr.p_proxy 	= NONE;
			WUNLOCK_PROC(xpp);

			sproxy_ptr->p_usr.p_getfrom = NONE;
			node_ptr->n_usr.n_stimestamp = current_kernel_time();
			node_ptr->n_usr.n_pxsent++;		
			
			sproxy_ptr->p_usr.p_rmtsent++;
			sproxy_ptr->p_usr.p_dcid = (-1);
			RUNLOCK_PROC(sproxy_ptr);

//			DC_DECREF(dc_ptr);
			return(OK);
		}
		
		sproxy_ptr->p_usr.p_getfrom = ANY;
		set_bit(BIT_RECEIVING, &sproxy_ptr->p_usr.p_rts_flags);
		DVKDEBUG(GENERIC,"Any message was not found.\n");
		if( timeout_ms == TIMEOUT_NOWAIT) {
			sproxy_ptr->p_usr.p_rts_flags = PROC_RUNNING;
			RUNLOCK_PROC(sproxy_ptr);
			ERROR_RETURN(EDVSTIMEDOUT);
		}

		ret = sleep_proc(sproxy_ptr, timeout_ms); 			/* SLEEP THE PROXY 	*/
		
		DVKDEBUG(GENERIC,"Someone wakes up the sender proxy\n");
		if(ret != OK) {
			sproxy_ptr->p_usr.p_rts_flags = PROC_RUNNING;
			RUNLOCK_PROC(sproxy_ptr);
			ERROR_RETURN(ret);
		}
	}
}	



