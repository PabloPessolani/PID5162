﻿/****************************************************************/
/*			MINIX IPC KERNEL 			*/
/* It uses the same Interrupt Vector than MINIX (0x21)		*/
/* It considers different Virtual Machines (Virtual Minix Envi-	*/
/* ronments or DCE) that let execute multiple Minix Over Linux	*/
/* on the same real machine					*/
/* MINIX IPC Calls: send, receive, sendrec, notify, vcopy 	*/
/* Hypervisor Calls: dc_init, dc_dump, proc_dump, bind, unbind  */
/* setpriv							*/ 
/****************************************************************/

#include "dvk_mod.h"

asmlinkage long new_hdw_notify(int dcid, int dst_ep);

#define PRINT_MAP(map) do {\
	printk("PRINT_MAP: %d:%s:%u:%s:",task_pid_nr(current), __FUNCTION__ ,__LINE__,#map);\
	for(i = 0; i < NR_SYS_CHUNKS; i++){\
		printk("%X.",(map.chunk)[i]);\
	}\
	printk("\n");\
}while(0);
	
	
/*2345678901234567890123456789012345678901234567890123456789012345678901234567*/

/*--------------------------------------------------------------*/
/*			new_mini_send				*/
/*--------------------------------------------------------------*/
asmlinkage long new_mini_send(int dst_ep, message* m_ptr, long timeout_ms)
{
	struct proc *dst_ptr, *caller_ptr, *sproxy_ptr;
	dc_desc_t *dc_ptr;
	int ret, retry;
	int caller_nr, caller_ep, caller_pid;
	int dst_nr, dcid;
	struct task_struct *task_ptr;

	DVKDEBUG(DBGPARAMS,"dst_ep=%d\n",dst_ep);

	if( DVS_NOT_INIT() ) ERROR_RETURN(EDVSDVSINIT);

	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret < 0) ERROR_RETURN(ret);

	/*------------------------------------------
	 * Checks the caller process PID
 	 * Gets the DCID
	 * Checks if the DC is RUNNING
      *------------------------------------------*/
 	RLOCK_PROC(caller_ptr);
	caller_nr   = caller_ptr->p_usr.p_nr;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	dcid		= caller_ptr->p_usr.p_dcid;
	DVKDEBUG(DBGPARAMS,"caller_nr=%d caller_ep=%d dst_ep=%d \n", caller_nr, caller_ep, dst_ep);
//if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADPID);
 	RUNLOCK_PROC(caller_ptr);

	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
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
	dst_nr = _ENDPOINT_P(dst_ep);
	if( dst_nr < (-dc_ptr->dc_usr.dc_nr_tasks)		
	 || dst_nr >= dc_ptr->dc_usr.dc_nr_procs)	
			ERROR_RETURN(EDVSRANGE)
	dst_ptr   = NBR2PTR(dc_ptr, dst_nr);
	if( dst_ptr == NULL) 				
		ERROR_RETURN(EDVSDSTDIED);
	if( caller_ep == dst_ep) 			
		ERROR_RETURN(EDVSENDPOINT);
	
 	WLOCK_PROC(caller_ptr);
	caller_ptr->p_umsg	= m_ptr;

send_replay: /* Return point for a migrated destination process */
	WLOCK_ORDERED2(caller_ptr,dst_ptr);
	/*------------------------------------------
	 * check the destination process status
	*------------------------------------------*/
	DVKDEBUG(DBGPARAMS,"dst_nr=%d dst_ep=%d\n",dst_nr, dst_ptr->p_usr.p_endpoint);
	do	{
		ret = OK;
		retry = 0;
		if (dst_ptr->p_usr.p_endpoint != dst_ep) 	
			{ret = EDVSENDPOINT; break;} 	/* Paranoid checking		*/
		if( test_bit(BIT_SLOT_FREE, &dst_ptr->p_usr.p_rts_flags))	
			{ret = EDVSDSTDIED; break;} 	/*destination died		*/	
		if( test_bit(BIT_MIGRATE, &dst_ptr->p_usr.p_rts_flags))	{
			set_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_waitmigr = dst_ep;
			INIT_LIST_HEAD(&caller_ptr->p_mlink);
			LIST_ADD_TAIL(&caller_ptr->p_mlink, &dst_ptr->p_mlist);
			sleep_proc2(caller_ptr, dst_ptr, timeout_ms);
			ret = caller_ptr->p_rcode;
			retry = 1;
			continue;
		} 	
		/* get destination nodeid */
		DVKDEBUG(INTERNAL,"dst_ptr->p_usr.p_nodeid=%d\n",dst_ptr->p_usr.p_nodeid);
		if( dst_ptr->p_usr.p_nodeid < 0 
			|| dst_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 
			{ret = EDVSBADNODEID; break;}	
		RLOCK_DC(dc_ptr);
		if( !test_bit(dst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	{
			ret = EDVSNODCNODE;
		}
		RUNLOCK_DC(dc_ptr);
	} while(retry);
	if(ret < 0) {							
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(ret);
	}
	
	// Set the message source endpoint 
	ret = put_user(caller_ep, &m_ptr->m_source);
	if( ret != OK) {							
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(ret);
	}
	
	DVKDEBUG(DBGPARAMS,"dcid=%d caller_pid=%d caller_nr=%d dst_ep=%d \n",
		caller_ptr->p_usr.p_dcid, caller_pid,caller_nr, dst_ep);
				
	caller_ptr->p_rcode= OK;
	if( IT_IS_REMOTE(dst_ptr)) {		/* the destination is REMOTE */
		/*---------------------------------------------------------------------------------------------------*/
		/*						REMOTE 								 */
		/*---------------------------------------------------------------------------------------------------*/
		sproxy_ptr = NODE2SPROXY(dst_ptr->p_usr.p_nodeid);
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY ;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
 		if(ret < 0) {							
			WUNLOCK_PROC2(caller_ptr, dst_ptr);
			ERROR_RETURN(ret);
		}

		clear_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags);
		if(	dst_ptr->p_usr.p_getfrom == caller_ptr->p_usr.p_endpoint){
			dst_ptr->p_usr.p_getfrom = NONE;
		}

		/* fill the caller's rmtcmd fields */
		caller_ptr->p_rmtcmd.c_cmd 		= CMD_SEND_MSG;
		caller_ptr->p_rmtcmd.c_src 		= caller_ep;
		caller_ptr->p_rmtcmd.c_dst 		= dst_ep;
		caller_ptr->p_rmtcmd.c_dcid		= caller_ptr->p_usr.p_dcid;
		caller_ptr->p_rmtcmd.c_snode  	= atomic_read(&local_nodeid);
		caller_ptr->p_rmtcmd.c_dnode  	= dst_ptr->p_usr.p_nodeid;
		caller_ptr->p_rmtcmd.c_rcode  	= OK;
		caller_ptr->p_rmtcmd.c_len  	= 0;
		WUNLOCK_PROC(dst_ptr); 

		COPY_FROM_USER_PROC(ret, (char*) &caller_ptr->p_rmtcmd.c_u.cu_msg, 
			m_ptr, sizeof(message) );
			
		INIT_LIST_HEAD(&caller_ptr->p_link);
		set_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_sendto = dst_ep;
		set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
		
		ret = sproxy_enqueue(caller_ptr);
			
		/* wait for the SENDACK */
		sleep_proc(caller_ptr, timeout_ms);
		ret = caller_ptr->p_rcode;
		if( ret == OK){
			caller_ptr->p_usr.p_rmtsent++;
		}else{
			if( test_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags)) {
				DVKDEBUG(GENERIC,"removing %d link from sender's proxy list.\n", 
					caller_ptr->p_usr.p_endpoint);
				RLOCK_PROC(sproxy_ptr);
				LIST_DEL(&caller_ptr->p_link); /* remove from queue ATENCION: HAY Q PROTEGER PROXY ?? */
				RUNLOCK_PROC(sproxy_ptr);
			}
			clear_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_sendto = NONE;
			clear_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_proxy = NONE;
			if( ret == EDVSMIGRATE) {
				WUNLOCK_PROC(caller_ptr); 
				goto send_replay;
			}
		}
	} else {					/* the destination is LOCAL  */
		/*---------------------------------------------------------------------------------------------------*/
		/*						LOCAL								 */
		/*---------------------------------------------------------------------------------------------------*/
		/* Check if 'dst' is blocked waiting for this message.   */
		if ( (test_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags) && 
				!test_bit(BIT_SENDING, &dst_ptr->p_usr.p_rts_flags)) &&
			(dst_ptr->p_usr.p_getfrom == ANY || dst_ptr->p_usr.p_getfrom == caller_ep)) {
			DVKDEBUG(GENERIC,"destination is waiting. Copy the message and wakeup destination\n");

			COPY_USR2USR_PROC(ret, caller_ep, caller_ptr, (char *) m_ptr, 
					dst_ptr, (char *)dst_ptr->p_umsg, sizeof(message) );
			if(ret < 0) {
				WUNLOCK_PROC2(caller_ptr, dst_ptr);
				ERROR_RETURN(ret); 
			}
			clear_bit(BIT_RECEIVING,&dst_ptr->p_usr.p_rts_flags);
			dst_ptr->p_usr.p_getfrom 	= NONE;
			if(dst_ptr->p_usr.p_rts_flags == 0) 
				LOCAL_PROC_UP(dst_ptr, ret);
			WUNLOCK_PROC(dst_ptr); 
			caller_ptr->p_usr.p_lclsent++;
		} else { 
			DVKDEBUG(GENERIC,"destination is not waiting dst_flags=%lX. Enqueue at the TAIL.\n"
				,dst_ptr->p_usr.p_rts_flags);
			/* The destination is not waiting for this message 			*/
			/* Append the caller at the TAIL of the destination senders' queue	*/
			/* blocked sending the message */
			set_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_sendto 	= dst_ep;
			caller_ptr->p_umsg		= m_ptr;
			INIT_LIST_HEAD(&caller_ptr->p_link);
			LIST_ADD_TAIL(&caller_ptr->p_link, &dst_ptr->p_list);

			sleep_proc2(caller_ptr, dst_ptr, timeout_ms);

			ret = caller_ptr->p_rcode;
			if( ret == OK){
				WUNLOCK_PROC(dst_ptr);
				caller_ptr->p_usr.p_lclsent++;
			}else{
				if( test_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags)) {
					DVKDEBUG(GENERIC,"removing %d link from %d list.\n", 
						caller_ptr->p_usr.p_endpoint, dst_ep);
					/* remove from queue ATENCION: HAY Q PROTEGER DESTINATION ?? */
					LIST_DEL(&caller_ptr->p_link); 
				}
				WUNLOCK_PROC(dst_ptr);
				clear_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
				caller_ptr->p_usr.p_sendto 	= NONE;
				if( ret == EDVSMIGRATE) {
					WUNLOCK_PROC(caller_ptr);
					goto send_replay;
				}
			}
		}
	}

	WUNLOCK_PROC(caller_ptr);
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/*--------------------------------------------------------------*/
/*			new_mini_receive			*/
/* Receives a message from another MOL process of the same DC	*/
/* Differences with MINIX:					*/
/*	- The receiver copies the message from sender's buffer  */
/*	   to receiver's userspace 				*/
/*	- After a the receiver is unblocked, it must check if 	*/
/*	   it was for doing a copy command (CMD_COPY_IN, CMD_COPY_OUT)	*/
/*--------------------------------------------------------------*/
asmlinkage long new_mini_receive(int src_ep, message* m_ptr, long timeout_ms)
{
	struct proc *caller_ptr, *xpp, *tmp_ptr, *src_ptr, *rproxy_ptr;
	dc_desc_t *dc_ptr;
	int sypriv_id, sys_nr, dcid;
//	int sys_ep = NONE;
	int i, ret, src_nr;
	dvk_map_t *map;
  	bitchunk_t *chunk;
	int caller_nr, caller_pid, caller_ep;
	struct task_struct *task_ptr;	
	struct timespec *t_ptr;
	message *mptr;


	if( DVS_NOT_INIT() ) 				ERROR_RETURN(EDVSDVSINIT);

	DVKDEBUG(DBGPARAMS,"src_ep=%d\n",src_ep);

	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret < 0) ERROR_RETURN(ret);
	
	/*------------------------------------------
	 * Checks the caller process PID
 	 * Gets the DCID
	 * Checks if the DC is RUNNING
	 * Locks the DC
	*------------------------------------------*/
	RLOCK_PROC(caller_ptr);
	caller_nr   = caller_ptr->p_usr.p_nr;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	DVKDEBUG(DBGPARAMS,"caller_nr=%d caller_ep=%d src_ep=%d \n", 
		caller_nr, caller_ep, src_ep);
//	if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//			ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADPID);
	dcid = caller_ptr->p_usr.p_dcid;
	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs.d_nr_dcs) 			
		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADDCID);
	dc_ptr 	= &dc[dcid];
	RUNLOCK_PROC(caller_ptr);

	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags)  
		ERROR_WUNLOCK_DC(dc_ptr, EDVSDCNOTRUN);
	RUNLOCK_DC(dc_ptr);	
	
	if( src_ep != ANY)	{
		src_nr = _ENDPOINT_P(src_ep);
		RLOCK_PROC(caller_ptr);
		/* DC does not need  to be locked because this fields are inmutable */
		if( src_nr < (-dc_ptr->dc_usr.dc_nr_tasks) 
		  || src_nr >= (dc_ptr->dc_usr.dc_nr_procs-dc_ptr->dc_usr.dc_nr_tasks)) 										
			ERROR_RUNLOCK_PROC(caller_ptr,EDVSRANGE);
		if( caller_ep == src_ep) 					
			ERROR_RUNLOCK_PROC(caller_ptr,EDVSENDPOINT);
		src_ptr = NBR2PTR(dc_ptr, src_nr);
		if( src_ptr == NULL) 
			ERROR_RUNLOCK_PROC(caller_ptr,EDVSSRCDIED);
		RUNLOCK_PROC(caller_ptr)

		RLOCK_PROC(src_ptr);
		if( test_bit(BIT_SLOT_FREE, &src_ptr->p_usr.p_rts_flags))
			ERROR_RUNLOCK_PROC(src_ptr,EDVSSRCDIED);
		if (src_ptr->p_usr.p_endpoint != src_ep)	
			ERROR_RUNLOCK_PROC(src_ptr,EDVSENDPOINT);

		if( src_ptr->p_usr.p_nodeid < 0 
			|| src_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 
			ERROR_RETURN(EDVSBADNODEID);
		
		if( IT_IS_REMOTE(src_ptr)) {
			RUNLOCK_PROC(src_ptr);
			/* verify if the RECEIVER PROXY of the process is CONNECTED and RUNNING */
			rproxy_ptr = NODE2RPROXY(src_ptr->p_usr.p_nodeid); 
			if( rproxy_ptr == NULL) 
				ERROR_RETURN(EDVSNOPROXY);

			RLOCK_PROC(rproxy_ptr);
			if( ! test_bit(src_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	
				ERROR_RUNLOCK_PROC(rproxy_ptr,EDVSNODCNODE);
			if( rproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				ERROR_RUNLOCK_PROC(rproxy_ptr,EDVSNOPROXY);
			if( test_bit( BIT_SLOT_FREE, &rproxy_ptr->p_usr.p_rts_flags))	
				ERROR_RUNLOCK_PROC(rproxy_ptr,EDVSNOPROXY);
			if( !test_bit(MIS_BIT_PROXY, &rproxy_ptr->p_usr.p_misc_flags))	
				ERROR_RUNLOCK_PROC(rproxy_ptr,EDVSNOPROXY);
			if( !test_bit(MIS_BIT_CONNECTED, &rproxy_ptr->p_usr.p_misc_flags))	
				ERROR_RUNLOCK_PROC(rproxy_ptr,EDVSNOTCONN);
			RUNLOCK_PROC(rproxy_ptr);
		} else {
			RUNLOCK_PROC(src_ptr);
		}
	}
	
	WLOCK_PROC(caller_ptr);
	
	/*--------------------------------------*/
	/* WAKE UP CONTROL 		*/
	/*--------------------------------------*/
	if( test_bit(MIS_BIT_WOKENUP, &caller_ptr->p_usr.p_misc_flags) && (src_ep == ANY)) {
		clear_bit(MIS_BIT_WOKENUP, &caller_ptr->p_usr.p_misc_flags);
		WUNLOCK_PROC(caller_ptr);
		ERROR_RETURN(EDVSWOKENUP);
	}
		
	/*--------------------------------------*/
	/* NOTIFY PENDING LOOP		*/
	/*--------------------------------------*/
	caller_ptr->p_umsg 	= m_ptr;
    map = &caller_ptr->p_priv.priv_notify_pending;
    for (chunk=&map->chunk[0]; chunk<&map->chunk[NR_SYS_CHUNKS]; chunk++) {
    	if (! *chunk) continue; 			/* no bits in chunk */
    	for (i=0; ! (*chunk & (1<<i)); ++i) {} 	/* look up the bit */
    	sypriv_id = (chunk - &map->chunk[0]) * BITCHUNK_BITS + i;
    	if (sypriv_id >= dc_ptr->dc_usr.dc_nr_sysprocs) break;		/* out of range */
		sys_nr = (sypriv_id-dc_ptr->dc_usr.dc_nr_tasks);	/* get source endpoint */
		src_ep = sys_nr;
		DVKDEBUG(INTERNAL  ,"sypriv_id=%d sys_nr=%d src_ep=%d\n", sypriv_id, sys_nr, src_ep);
		if ( (src_ep != ANY) && (_ENDPOINT_P(src_ep) != sys_nr) ) 
			continue;/* source not ok */
		*chunk &= ~(1 << i);			/* no longer pending */
		DVKDEBUG(INTERNAL ,"deliver the notification message sypriv_id=%d src_ep=%d\n", 
			sypriv_id, src_ep);
	   	/* Found a suitable source, deliver the notification message. */
		src_ptr = NBR2PTR(dc_ptr, sys_nr);
		BUILD_NOTIFY_MSG(src_ptr, src_ep, sys_nr, caller_ptr);
		t_ptr = &caller_ptr->p_message.m9_t1;
		DVKDEBUG(INTERNAL,TIME_FORMAT,TIME_FIELDS(t_ptr) );
		COPY_TO_USER_PROC(ret, (void *)&caller_ptr->p_message, 
			(void *)m_ptr,  sizeof(message));
		WUNLOCK_PROC(caller_ptr);
		if (ret < 0) ERROR_RETURN(EDVSMSGSIZE);
		return(OK);	/* report success */
	}

	/*-----------------------------------------*/
	/* MESSAGE RECEIVE 		*/
	/*-----------------------------------------*/

	LIST_FOR_EACH_ENTRY_SAFE(xpp, tmp_ptr, &caller_ptr->p_list, p_link) {
		if( caller_ptr->p_usr.p_nr < xpp->p_usr.p_nr) {
			WLOCK_PROC(xpp);
		}else{
			WUNLOCK_PROC(caller_ptr);
			WLOCK_PROC(xpp);
			WLOCK_PROC(caller_ptr);
		}
		if (src_ep == xpp->p_usr.p_endpoint || src_ep == ANY ) {
			DVKDEBUG(GENERIC,"Found acceptable message from %d. Copy it and update status.\n"
				,xpp->p_usr.p_endpoint );
			/* Here is a message from xpp process, therefore xpp  must be sleeping in SENDING state */

			LIST_DEL(&xpp->p_link); /* remove from queue */

			/* test the sender status */
			do	{
				if( test_bit(BIT_SLOT_FREE, &xpp->p_usr.p_rts_flags))	
					{ret = EDVSSRCDIED; break;} 	
				if( !test_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags))	
					{ret = EDVSPROCSTS; break;} 	
				if( xpp->p_usr.p_sendto != caller_ptr->p_usr.p_endpoint) 	
					{ret = EDVSPROCSTS; break;} 
				if( (src_ep != ANY) && (xpp->p_usr.p_endpoint != src_ep))	
					{ret = EDVSENDPOINT;break;}
				ret = OK;
			} while(0);
		
			if(ret == OK) {			
				if( IT_IS_REMOTE(xpp) ){ 
					COPY_TO_USER_PROC(ret, (void *)&xpp->p_message, 
						(void *) m_ptr, sizeof(message));
		 			if( !test_bit(BIT_RECEIVING, &xpp->p_usr.p_rts_flags)) {
						send_ack_lcl2rmt(xpp, caller_ptr, ret);
					}
				}else {
					COPY_USR2USR_PROC(ret, xpp->p_usr.p_endpoint, xpp, (char *)xpp->p_umsg,
						caller_ptr, (char *)m_ptr, sizeof(message) );
				} 
				clear_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags);
				xpp->p_usr.p_sendto 	= NONE;
				if(xpp->p_usr.p_rts_flags == 0) 
					LOCAL_PROC_UP(xpp, ret);
			}
				
			WUNLOCK_PROC(xpp);
			WUNLOCK_PROC(caller_ptr);
			if( ret < 0 ) ERROR_RETURN(ret);
			return(OK);
		}else{
			WUNLOCK_PROC(xpp);
			DVKDEBUG(GENERIC,"Found a message from %d but not acceptable \n"
				,xpp->p_usr.p_endpoint );	
		}
	}
		
	set_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
	clear_bit(MIS_BIT_NOTIFY, &caller_ptr->p_usr.p_misc_flags);
	caller_ptr->p_usr.p_getfrom 	= src_ep;
	caller_ptr->p_rcode		= OK;
	DVKDEBUG(GENERIC,"Any suitable message from %d was not found.\n", src_ep);	

	sleep_proc(caller_ptr, timeout_ms);
		
	ret = caller_ptr->p_rcode;
	if( ret != OK){
		clear_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_getfrom 	= NONE;
	}else {
		if(test_bit(MIS_BIT_NOTIFY,&caller_ptr->p_usr.p_misc_flags)){
			COPY_TO_USER_PROC(ret, &caller_ptr->p_message, caller_ptr->p_umsg, sizeof(message) );
			mptr = &caller_ptr->p_message;
			DVKDEBUG(DBGMESSAGE, MSG9_FORMAT, MSG9_FIELDS(mptr));			
		}
	}
	clear_bit(MIS_BIT_NOTIFY, &caller_ptr->p_usr.p_misc_flags);

	WUNLOCK_PROC(caller_ptr);
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}	



/*--------------------------------------------------------------*/
/*			new_mini_sendrec			*/
/*--------------------------------------------------------------*/
asmlinkage long new_mini_sendrec(int srcdst_ep, message* m_ptr, long timeout_ms)
{
	struct proc *srcdst_ptr, *caller_ptr, *sproxy_ptr;
	dc_desc_t *dc_ptr;
	int ret, retry, dcid, ret1;
	int caller_nr, caller_pid, caller_ep;
	int srcdst_nr;
	struct task_struct *task_ptr;	

	DVKDEBUG(DBGPARAMS,"srcdst_ep=%d\n", srcdst_ep);
	if( DVS_NOT_INIT() ) 				ERROR_RETURN(EDVSDVSINIT);

	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret < 0) ERROR_RETURN(ret);

	/*------------------------------------------
	 * Checks the caller process PID
 	 * Gets the DCID
	 * Checks if the DC is RUNNING
      *------------------------------------------*/
 	RLOCK_PROC(caller_ptr);
	caller_nr   = caller_ptr->p_usr.p_nr;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	dcid		= caller_ptr->p_usr.p_dcid;
	DVKDEBUG(DBGPARAMS,"caller_nr=%d caller_ep=%d srcdst_ep=%d \n", 
		caller_nr, caller_ep, srcdst_ep);
//	if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADPID);
 	RUNLOCK_PROC(caller_ptr);

	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
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
	srcdst_nr = _ENDPOINT_P(srcdst_ep);
	if( srcdst_nr < (-dc_ptr->dc_usr.dc_nr_tasks)		
	 || srcdst_nr >= dc_ptr->dc_usr.dc_nr_procs)	
		ERROR_RETURN(EDVSRANGE)
	srcdst_ptr   = NBR2PTR(dc_ptr, srcdst_nr);
	if( srcdst_ptr == NULL) 				
		ERROR_RETURN(EDVSDSTDIED);
	if( caller_ep == srcdst_ep) 			
		ERROR_RETURN(EDVSENDPOINT);

	WLOCK_PROC(caller_ptr);
	caller_ptr->p_umsg	= m_ptr;
	
sendrec_replay:
	/*------------------------------------------
	 * check the destination process status
	*------------------------------------------*/
	WLOCK_ORDERED2(caller_ptr,srcdst_ptr);
	DVKDEBUG(DBGPARAMS,"srcdst_nr=%d srcdst_ep=%d\n",srcdst_nr, srcdst_ptr->p_usr.p_endpoint);

	do	{
		ret = OK;
		retry = 0;
		if (srcdst_ptr->p_usr.p_endpoint != srcdst_ep) 			
			{ret = EDVSENDPOINT; break;} 	/* Paranoid checking	*/
		if (test_bit(BIT_SLOT_FREE, &srcdst_ptr->p_usr.p_rts_flags))	
			{ret = EDVSDEADSRCDST; break;} 	/*destination died		*/
		if( test_bit(BIT_MIGRATE, &srcdst_ptr->p_usr.p_rts_flags))	{
			set_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_waitmigr = srcdst_ep;
			INIT_LIST_HEAD(&caller_ptr->p_mlink);
			LIST_ADD_TAIL(&caller_ptr->p_mlink, &srcdst_ptr->p_mlist);
			sleep_proc2(caller_ptr, srcdst_ptr, timeout_ms);
			ret = caller_ptr->p_rcode;
			retry = 1;
			continue;
		} 	
		/* get destination nodeid */
		DVKDEBUG(INTERNAL,"srcdst_ptr->p_usr.p_nodeid=%d\n",srcdst_ptr->p_usr.p_nodeid);
		if( srcdst_ptr->p_usr.p_nodeid < 0 
			|| srcdst_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 
			{ret = EDVSBADNODEID; break;}	
		RLOCK_DC(dc_ptr);
		if( !test_bit(srcdst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	{
			ret = EDVSNODCNODE;
		}
		RUNLOCK_DC(dc_ptr);
	} while(retry);
	if(ret < 0) {							
		WUNLOCK_PROC2(caller_ptr, srcdst_ptr);
		ERROR_RETURN(ret);
	}

	// Set the message source endpoint 
	ret = put_user(caller_ep, &m_ptr->m_source);
	if( ret < OK) {							
		WUNLOCK_PROC2(caller_ptr, srcdst_ptr);
		ERROR_RETURN(ret);
	}
	
	DVKDEBUG(DBGPARAMS,"dcid=%d caller_pid=%d caller_nr=%d srcdst_ep=%d \n",
		caller_ptr->p_usr.p_dcid, caller_pid,caller_nr, srcdst_ep);

	/*--------------------------------------*/
	/* SENDING/RECEIVING		*/
	/*--------------------------------------*/
	DVKDEBUG(GENERIC,"SENDING HALF\n");

	caller_ptr->p_rcode	= OK;
	caller_ptr->p_usr.p_getfrom  = srcdst_ep;

	if( IT_IS_REMOTE(srcdst_ptr)) {		/* the destination is REMOTE */
		/*---------------------------------------------------------------------------------------------------*/
		/*						REMOTE 								 */
		/*---------------------------------------------------------------------------------------------------*/

		sproxy_ptr = NODE2SPROXY(srcdst_ptr->p_usr.p_nodeid); 
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( ! test_bit(srcdst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	
				{ret = EDVSNODCNODE ;break;}
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
		if(ret < 0) {							
			WUNLOCK_PROC2(caller_ptr, srcdst_ptr);
			ERROR_RETURN(ret);
		}

		/* fill the caller's rmtcmd fields */
		caller_ptr->p_rmtcmd.c_cmd 		= CMD_SNDREC_MSG;
		caller_ptr->p_rmtcmd.c_src 		= caller_ep;
		caller_ptr->p_rmtcmd.c_dst 		= srcdst_ep;
		caller_ptr->p_rmtcmd.c_dcid		= caller_ptr->p_usr.p_dcid;
		caller_ptr->p_rmtcmd.c_snode  	= atomic_read(&local_nodeid);
		caller_ptr->p_rmtcmd.c_dnode  	= srcdst_ptr->p_usr.p_nodeid;
		caller_ptr->p_rmtcmd.c_rcode  	= OK;
		caller_ptr->p_rmtcmd.c_len  	= 0;
		WUNLOCK_PROC(srcdst_ptr);
		COPY_FROM_USER_PROC(ret, (char*) &caller_ptr->p_rmtcmd.c_u.cu_msg, 
			m_ptr, sizeof(message) );
		
		INIT_LIST_HEAD(&caller_ptr->p_link);
		set_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
		set_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
		set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_sendto  = srcdst_ep;
		caller_ptr->p_usr.p_getfrom = srcdst_ep;
		
		ret = sproxy_enqueue(caller_ptr);

		/* wait for the REPLY */
		sleep_proc(caller_ptr, timeout_ms);

		ret = caller_ptr->p_rcode;
		if( ret == OK){
			caller_ptr->p_usr.p_rmtsent++;
		}else{
			if( test_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags)) {
				DVKDEBUG(GENERIC,"removing %d link from sender's proxy list.\n",
					 caller_ptr->p_usr.p_endpoint);
				LIST_DEL(&caller_ptr->p_link); /* remove from queue ATENCION: HAY Q PROTEGER PROXY ?? */
			}
			clear_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
			clear_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
			clear_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_sendto  = NONE;
			caller_ptr->p_usr.p_getfrom = NONE;
			caller_ptr->p_usr.p_proxy = NONE;
			if( ret == EDVSMIGRATE) {
				WUNLOCK_PROC(caller_ptr);
				goto sendrec_replay;
			}
		}
	} else {
		/*---------------------------------------------------------------------------------------------------*/
		/*						LOCAL  								 */
		/*---------------------------------------------------------------------------------------------------*/
		/* Check if 'dst' is blocked waiting for this message.   */
		if (  (test_bit(BIT_RECEIVING, &srcdst_ptr->p_usr.p_rts_flags) 
			&& !test_bit(BIT_SENDING, &srcdst_ptr->p_usr.p_rts_flags)) &&
			(srcdst_ptr->p_usr.p_getfrom == ANY || srcdst_ptr->p_usr.p_getfrom == caller_ep)) {
			DVKDEBUG(GENERIC,"destination is waiting. Copy the message and wakeup destination\n");
			clear_bit(BIT_RECEIVING, &srcdst_ptr->p_usr.p_rts_flags);
			srcdst_ptr->p_usr.p_getfrom 	= NONE;

			COPY_USR2USR_PROC(ret, caller_ep, caller_ptr, (char *) m_ptr, srcdst_ptr, (char *) srcdst_ptr->p_umsg, sizeof(message) );
			if(srcdst_ptr->p_usr.p_rts_flags == 0) 
				LOCAL_PROC_UP(srcdst_ptr, ret); 
			if(ret < 0) {
				caller_ptr->p_usr.p_getfrom  	= NONE;
				caller_ptr->p_usr.p_sendto 	= NONE;
				WUNLOCK_PROC2(caller_ptr, srcdst_ptr);
				ERROR_RETURN(ret);
			}
			set_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags); /* Sending part: completed, now receiving.. */
			WUNLOCK_PROC(srcdst_ptr);
			sleep_proc(caller_ptr, timeout_ms); 
			ret = caller_ptr->p_rcode;
			if( ret) {
				clear_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
				clear_bit(BIT_SENDING,   &caller_ptr->p_usr.p_rts_flags);
				caller_ptr->p_usr.p_getfrom    	= NONE;
				caller_ptr->p_usr.p_sendto 	= NONE;
			}else {
				caller_ptr->p_usr.p_lclsent++;
			}
		} else if ( test_bit(BIT_SENDING, &srcdst_ptr->p_usr.p_rts_flags) &&
			srcdst_ptr->p_usr.p_sendto == caller_ep) {
			// this situation occurs when the caller exits from previous sendrec with ERESTARTSYS  and the srcdst process want to reply_replay 
			// the previous request to caller.
			DVKDEBUG(GENERIC,"destination %d is waiting to send to %d\n", srcdst_ep,caller_ep);
			COPY_USR2USR_PROC(ret, caller_ep, srcdst_ptr, (char *) srcdst_ptr->p_umsg, caller_ptr, (char *) m_ptr,  sizeof(message) );
			clear_bit(BIT_SENDING, &srcdst_ptr->p_usr.p_rts_flags);
			srcdst_ptr->p_usr.p_sendto 	= NONE;
			if(srcdst_ptr->p_usr.p_rts_flags == 0) {
				LOCAL_PROC_UP(srcdst_ptr, ret1); 
				if(ret1 < EDVSERRCODE)
					ERROR_PRINT(ret1);
			}
			if(ret < 0) {
				caller_ptr->p_usr.p_getfrom = NONE;
				caller_ptr->p_usr.p_sendto 	= NONE;
				WUNLOCK_PROC2(caller_ptr, srcdst_ptr);
				ERROR_RETURN(ret);
			}else {
				srcdst_ptr->p_usr.p_lclsent++;
			}
			WUNLOCK_PROC(srcdst_ptr);				
		}else { 
			DVKDEBUG(GENERIC,"destination is not waiting to receive srcdst_ptr-flags=%lX. Enqueue at TAIL.\n"
				,srcdst_ptr->p_usr.p_rts_flags);
			/* The destination is not waiting for this message 			*/
			/* Append the caller at the TAIL of the destination senders' queue	*/
			/* blocked sending the message */


			caller_ptr->p_message.m_source = caller_ptr->p_usr.p_endpoint;
			set_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
			set_bit(BIT_SENDING,   &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_sendto 	= srcdst_ep;	
			INIT_LIST_HEAD(&caller_ptr->p_link);
			LIST_ADD_TAIL(&caller_ptr->p_link, &srcdst_ptr->p_list);
			sleep_proc2(caller_ptr, srcdst_ptr, timeout_ms); 
			ret = caller_ptr->p_rcode;

			if( ret) {
				if( test_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags)){
					DVKDEBUG(GENERIC,"removing %d link from %d list.\n", caller_ptr->p_usr.p_endpoint, srcdst_ep);
					LIST_DEL(&caller_ptr->p_link); /* remove from queue */
				} else {
					caller_ptr->p_usr.p_lclsent++;
				}
				WUNLOCK_PROC(srcdst_ptr);
				clear_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
				clear_bit(BIT_SENDING,   &caller_ptr->p_usr.p_rts_flags);
				caller_ptr->p_usr.p_getfrom = NONE;
				caller_ptr->p_usr.p_sendto 	= NONE;
				if( ret == EDVSMIGRATE){ 
					WUNLOCK_PROC(caller_ptr);
					goto sendrec_replay;
				}
			} else {
				caller_ptr->p_usr.p_lclsent++;
				WUNLOCK_PROC(srcdst_ptr);
			}
		}
	}

	WUNLOCK_PROC(caller_ptr);
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/*--------------------------------------------------------------*/
/*			new_mini_notify				*/
/*--------------------------------------------------------------*/
asmlinkage long new_mini_notify(int src_nr, int dst_ep, int update_proc)
{

	struct proc *dst_ptr, *caller_ptr, *sproxy_ptr, *src_ptr;
	dc_desc_t *dc_ptr;
	int caller_nr, caller_pid, caller_ep, src_ep;
	int dst_nr, dcid;
	int ret;
	struct task_struct *task_ptr;	
	struct timespec *t_ptr;
	message *mptr;

	DVKDEBUG(DBGPARAMS,"src_nr=%d dst_ep=%d update_proc=%d \n", src_nr, dst_ep, update_proc);
	
	if( DVS_NOT_INIT() ) 			ERROR_RETURN(EDVSDVSINIT);

	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	
	/* Admit sending notify from an unbound process in name of HARWARE */
	if( src_nr == HARDWARE) {
		// stub_syscall3(NOTIFY, HARDWARE, dst_ep, dcid)
		ret = new_hdw_notify(update_proc, dst_ep); // update_proc is the dcid for this function 
		if(ret < 0) ERROR_RETURN (ret);
		return(ret);
	}
	if(ret < 0) ERROR_RETURN (ret);
	/*------------------------------------------
	 * Checks the caller process PID
	 * Gets the DCID
	 * Checks if the DC is RUNNING
	  *------------------------------------------*/
	RLOCK_PROC(caller_ptr);
	caller_nr   = caller_ptr->p_usr.p_nr;
	dcid		= caller_ptr->p_usr.p_dcid;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	DVKDEBUG(DBGPARAMS,"caller_nr=%d caller_ep=%d dst_ep=%d \n", caller_nr, caller_ep, dst_ep);
//	if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADPID);
	RUNLOCK_PROC(caller_ptr);
		
	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs.d_nr_dcs) 			
		ERROR_RETURN(EDVSBADDCID);
	dc_ptr 	= &dc[dcid];
	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags)  
		ERROR_RUNLOCK_DC(dc_ptr,EDVSDCNOTRUN);
	if( src_nr == SELF){
		src_nr = caller_nr;
		src_ep = caller_ep;
	}else{
		src_ptr = NBR2PTR(dc_ptr, src_nr);
		RLOCK_PROC(src_ptr);
		src_ep = src_ptr->p_usr.p_endpoint;
		RUNLOCK_PROC(src_ptr);
	}
	RUNLOCK_DC(dc_ptr);	

	/*------------------------------------------
	 * get the destination process number
	*------------------------------------------*/
	dst_nr = _ENDPOINT_P(dst_ep);
	if( dst_nr < (-dc_ptr->dc_usr.dc_nr_tasks)		
	 || dst_nr >= dc_ptr->dc_usr.dc_nr_procs)	
		ERROR_RETURN(EDVSRANGE)
	dst_ptr   = NBR2PTR(dc_ptr, dst_nr);
	if( dst_ptr == NULL) 				
		ERROR_RETURN(EDVSDSTDIED);
	if( caller_ep == dst_ep) 			
		ERROR_RETURN(EDVSENDPOINT);

// 	RLOCK_PROC(dst_ptr);
//	if( dst_ptr->p_priv.priv_usr.priv_id >= dc_ptr->dc_usr.dc_nr_sysprocs) 
//		ERROR_RUNLOCK_PROC(dst_ptr,EDVSPRIVILEGES);
//	RUNLOCK_PROC(dst_ptr);

 	WLOCK_PROC(caller_ptr);
	/** Checks the caller's privileges */
	if( caller_ptr->p_priv.priv_usr.priv_id >= dc_ptr->dc_usr.dc_nr_sysprocs)
		ERROR_WUNLOCK_PROC(caller_ptr,EDVSPRIVILEGES);
	
notify_replay:

	/*------------------------------------------
	 * check the destination process status
	*------------------------------------------*/
	WLOCK_ORDERED2(caller_ptr,dst_ptr);
	DVKDEBUG(DBGPARAMS,"dst_nr=%d dst_ep=%d\n",dst_nr, dst_ptr->p_usr.p_endpoint);
	if (dst_ptr->p_usr.p_endpoint != dst_ep) 	{
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(EDVSENDPOINT);
	}
	if( test_bit(BIT_SLOT_FREE, &dst_ptr->p_usr.p_rts_flags)){
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(EDVSDSTDIED);
	}
	if( dst_ptr->p_usr.p_nodeid < 0 
		|| dst_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 	 {
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(EDVSBADNODEID);
	} 
	if( ! test_bit(dst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)){ 	
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(EDVSNODCNODE);
	}
	if( test_bit(BIT_MIGRATE, &dst_ptr->p_usr.p_rts_flags)) {	/*destination is migrating	*/
		DVKDEBUG(GENERIC,"destination is migrating dst_ptr->p_usr.p_rts_flags=%lX\n"
			,dst_ptr->p_usr.p_rts_flags);
  		/* Add to destination the bit map with pending notifications  */
		/* ATENCION-WARNING_ Revisar que estas notificaciones pendientes se envien al terminar la migracion  */
		if(get_sys_bit(dst_ptr->p_priv.priv_notify_pending, caller_ptr->p_priv.priv_usr.priv_id)){
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(EDVSOVERRUN);
		}
			
		DVKDEBUG(INTERNAL,"set_sys_bit caller_ptr->p_priv.priv_usr.priv_id=%d\n",
				caller_ptr->p_priv.priv_usr.priv_id);
		set_sys_bit(dst_ptr->p_priv.priv_notify_pending, caller_ptr->p_priv.priv_usr.priv_id);
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		return(OK);
	}
	
	DVKDEBUG(DBGPARAMS,"dcid=%d caller_pid=%d caller_nr=%d dst_ep=%d \n",
		caller_ptr->p_usr.p_dcid, caller_pid,caller_nr, dst_ep);

	caller_ptr->p_rcode= OK;
	if( IT_IS_REMOTE(dst_ptr)) {		/* the destination is REMOTE */
		/*---------------------------------------------------------------------------------------------------*/
		/*						REMOTE  								 */
		/*---------------------------------------------------------------------------------------------------*/	

		sproxy_ptr = NODE2SPROXY(dst_ptr->p_usr.p_nodeid); 
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
			{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
 		if(ret < 0) {							
			WUNLOCK_PROC2(caller_ptr, dst_ptr);
			ERROR_RETURN(ret);
		}

		clear_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags);
		if(	dst_ptr->p_usr.p_getfrom == caller_ptr->p_usr.p_endpoint){
			dst_ptr->p_usr.p_getfrom = NONE;
		}

		/* fill the caller's rmtcmd fields */
		caller_ptr->p_rmtcmd.c_cmd 		= CMD_NTFY_MSG;
		caller_ptr->p_rmtcmd.c_src 		= caller_ep;
		caller_ptr->p_rmtcmd.c_dst 		= dst_ep;
		caller_ptr->p_rmtcmd.c_dcid		= caller_ptr->p_usr.p_dcid;
		caller_ptr->p_rmtcmd.c_snode  	= atomic_read(&local_nodeid);
		caller_ptr->p_rmtcmd.c_dnode  	= dst_ptr->p_usr.p_nodeid;
		if( update_proc >= 0 &&  update_proc < (sizeof(update_t)*8)) 
			caller_ptr->p_rmtcmd.c_rcode  	= (int) update_proc;
		else	
			caller_ptr->p_rmtcmd.c_rcode  	= (-1);
		caller_ptr->p_rmtcmd.c_len  	= 0;
		
		// build the message in dst_buffer (remote process)
		BUILD_NOTIFY_MSG(caller_ptr, src_ep, src_nr, dst_ptr);
		WUNLOCK_PROC(dst_ptr);
		
		// copy message from destination buffer to caller buffer 
		memcpy(&caller_ptr->p_rmtcmd.c_u.cu_msg, 
			   &dst_ptr->p_message, 
			   sizeof(message) );
		
		INIT_LIST_HEAD(&caller_ptr->p_link);
		set_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
		set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_sendto = dst_ep;
		
		ret = sproxy_enqueue(caller_ptr);

		/* wait that the proxy wakes up to free the caller's descriptor  */
		sleep_proc(caller_ptr, TIMEOUT_FOREVER);

		ret = caller_ptr->p_rcode;
		if( ret == OK){
			caller_ptr->p_usr.p_rmtsent++;
		}else{
			if( test_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags)) {
				DVKDEBUG(GENERIC,"removing %d link from sender's proxy list.\n", 
					caller_ptr->p_usr.p_endpoint);
				LIST_DEL(&caller_ptr->p_link); /* remove from queue ATENCION: HAY Q PROTEGER PROXY ?? */
			}
			clear_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
			clear_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_sendto = NONE;
			caller_ptr->p_usr.p_proxy = NONE;
			if( ret == EDVSMIGRATE) {
				WUNLOCK_PROC(caller_ptr);
				goto notify_replay;
			}
		}
	} else {							/* Destination is LOCAL */
		/*---------------------------------------------------------------------------------------------------*/
		/*						LOCAL  								 */
		/*---------------------------------------------------------------------------------------------------*/
		/* Check if 'dst' is blocked waiting for this message.   */
		if (  (test_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags) 
				&& !test_bit(BIT_SENDING, &dst_ptr->p_usr.p_rts_flags) )&&
			(dst_ptr->p_usr.p_getfrom == ANY || dst_ptr->p_usr.p_getfrom == src_ep)) {
			DVKDEBUG(GENERIC,"destination is waiting. Build the message and wakeup destination\n");

			/* Build the message in the destination buffer */
			if( update_proc >= 0 &&  update_proc < (sizeof(update_t)*8)) {
				DVKDEBUG(INTERNAL,"set_bit update_proc=%d\n",update_proc);
				set_bit(update_proc, &caller_ptr->p_priv.priv_updt_pending ); 
			}
			
			BUILD_NOTIFY_MSG(caller_ptr, src_ep, src_nr, dst_ptr); 
			mptr = &dst_ptr->p_message;
			DVKDEBUG(DBGMESSAGE, MSG9_FORMAT, MSG9_FIELDS(mptr));			

			set_bit(MIS_BIT_NOTIFY, &dst_ptr->p_usr.p_misc_flags);

			t_ptr = &caller_ptr->p_message.m9_t1;
			DVKDEBUG(GENERIC,TIME_FORMAT,TIME_FIELDS(t_ptr) );
			clear_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags);
			dst_ptr->p_usr.p_getfrom 	= NONE;
			if(dst_ptr->p_usr.p_rts_flags == 0)
				LOCAL_PROC_UP(dst_ptr, OK); /* ATENTI puede haber mas de una razon para despertar al proceso!! */
		} else { 
			if( dst_ptr->p_priv.priv_usr.priv_id >= dc_ptr->dc_usr.dc_nr_sysprocs) 
				ERROR_WUNLOCK_PROC(dst_ptr,EDVSPRIVILEGES);
	
			DVKDEBUG(GENERIC,"destination is not waiting dst_ptr->p_usr.p_rts_flags=%lX\n",dst_ptr->p_usr.p_rts_flags);
  			/* Add to destination the bit map with pending notifications  */
			if(get_sys_bit(dst_ptr->p_priv.priv_notify_pending, caller_ptr->p_priv.priv_usr.priv_id))
				ERROR_PRINT(EDVSOVERRUN);
			DVKDEBUG(INTERNAL,"set_sys_bit caller_ptr->p_priv.priv_usr.priv_id=%d\n",caller_ptr->p_priv.priv_usr.priv_id);
			set_sys_bit(dst_ptr->p_priv.priv_notify_pending, caller_ptr->p_priv.priv_usr.priv_id); 
			if( update_proc >= 0 &&  update_proc < (sizeof(update_t)*8)) {
				DVKDEBUG(INTERNAL,"set_bit update_proc=%d\n",update_proc);
				set_bit(update_proc, &caller_ptr->p_priv.priv_updt_pending); 
			}
		}
		WUNLOCK_PROC(dst_ptr);
	}

	ret = caller_ptr->p_rcode;
	if(ret == OK){
		if( IT_IS_REMOTE(dst_ptr)) 	caller_ptr->p_usr.p_rmtsent++;
		else						caller_ptr->p_usr.p_lclsent++;
	} else { /* Only for remote destination */ 
		clear_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_sendto 	= NONE;
	}

	WUNLOCK_PROC(caller_ptr);
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/*--------------------------------------------------------------*/
/*			new_mini_relay				*/
/*--------------------------------------------------------------*/
asmlinkage long new_mini_relay(int dst_ep, message* m_ptr)
{
	return(OK);
}

/*----------------------------------------------------------------*/
/*			new_vcopy				*/
/* This function is used to:			*/
/*  - Copy messages: when src_ep =! NONE and bytes=sizeof(message) */
/*   - Copy data blocks: when src_ep == NONE	*/
/*----------------------------------------------------------------*/

asmlinkage long new_vcopy(int src_ep, char *src_addr, int dst_ep,char *dst_addr, int bytes)
{
	struct proc *src_ptr, *dst_ptr, *caller_ptr, *sproxy_ptr;
	int src_nr, dst_nr, caller_nr;
	int caller_pid, dcid;
	int src_pid , src_tgid;
	int dst_pid , dst_tgid, nr_tasks;
	dc_desc_t *dc_ptr;
	int caller_ep;
	int copylen;
	int retry, ret = OK;
	struct task_struct *task_ptr;
	cmd_t *cmd_ptr;
	
	DVKDEBUG(DBGPARAMS,"src_ep=%d dst_ep=%d bytes=%d\n",src_ep, dst_ep, bytes);

#ifdef DVS_CAPABILITIES
	 if ( !capable(CAP_VOS_ADMIN)) ERROR_RETURN(EDVSPRIVILEGES);
#else
	if(get_current_cred()->euid.val != USER_ROOT) ERROR_RETURN(EDVSPRIVILEGES);
#endif 

	if( bytes < 0 || bytes  > dvs.d_max_copylen) ERROR_RETURN(EDVSRANGE);
 
	if( DVS_NOT_INIT() ) 		ERROR_RETURN(EDVSDVSINIT);

	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret < 0) ERROR_RETURN(ret);
	RLOCK_PROC(caller_ptr);

	if( src_ep == SELF) src_ep = caller_ptr->p_usr.p_endpoint;
	else if( dst_ep == SELF) dst_ep = caller_ptr->p_usr.p_endpoint;

	if( (src_ep == dst_ep) 	|| 	(src_ep == ANY)	|| 
		(dst_ep == ANY) ||	(src_ep == NONE)|| 
		(dst_ep == NONE) )	{
			RUNLOCK_PROC(caller_ptr);
			ERROR_RETURN(EDVSENDPOINT);
	}

	/*------------------------------------------
	 * Checks the caller process 
 	 * Gets the DCID
	 * Checks if the DC is RUNNING
      *------------------------------------------*/
//	if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//		ERROR_RUNLOCK_PROC(caller_ptr, EDVSBADPID);
		
	dcid = caller_ptr->p_usr.p_dcid;
	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs.d_nr_dcs) 		
		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADDCID);
		
	dc_ptr 	= &dc[dcid];
	RUNLOCK_PROC(caller_ptr);
	
	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags)  
		ERROR_RUNLOCK_DC(dc_ptr, EDVSDCNOTRUN);

	/*-------------------------------------------------------------*/
	/*	get info about SOURCE and DESTINATION processes */
	/*-------------------------------------------------------------*/
    nr_tasks = dc_ptr->dc_usr.dc_nr_tasks;
	src_nr  = _ENDPOINT_P(src_ep);
	if( 	src_nr < (-dc_ptr->dc_usr.dc_nr_tasks) 
		|| 	src_nr >= dc_ptr->dc_usr.dc_nr_procs) {
		ERROR_RUNLOCK_DC(dc_ptr, EDVSRANGE);
	}
	src_ptr = NBR2PTR(dc_ptr,src_nr);


	dst_nr  = _ENDPOINT_P(dst_ep);
	if( 	dst_nr < (-dc_ptr->dc_usr.dc_nr_tasks) 
		|| 	dst_nr >= dc_ptr->dc_usr.dc_nr_procs) {
		ERROR_RUNLOCK_DC(dc_ptr, EDVSRANGE);
	}
	dst_ptr = NBR2PTR(dc_ptr, dst_nr);
	
	/*-------------------------------------------------------------*/
	/*	LOCK PROCESSES IN ASCENDENT ORDER		*/
	/*-------------------------------------------------------------*/
	DVKDEBUG(GENERIC,"LOCK PROCESSES IN ASCENDENT ORDER\n");
	if( src_ptr != caller_ptr && dst_ptr != caller_ptr ) {	/* Requester is a third process */
		WLOCK_PROC3(caller_ptr,src_ptr,dst_ptr);
	}else {
		if( dst_ptr != caller_ptr) {					/* requester is the source	*/
			WLOCK_PROC2(caller_ptr,dst_ptr);
		}else{
			WLOCK_PROC2(caller_ptr,src_ptr);			/* requester is the destination */
		}
	}
	
	DVKDEBUG(GENERIC,"CHECK FOR SOURCE/DESTINATION STATUS\n");
	do	{
		retry = 0;
		ret = OK;
		/*--------------------------------------------------------------*/
		/*			check SOURCE										*/
		/*--------------------------------------------------------------*/
		if(src_ptr->p_usr.p_endpoint != src_ep) 					
			{ret = EDVSENDPOINT; break;} 	/* Paranoid checking		*/
		if(test_bit(BIT_SLOT_FREE, &src_ptr->p_usr.p_rts_flags))	
			{ret = EDVSSRCDIED; break;} 	/* source  died			*/
		if(test_bit(BIT_ONCOPY, &src_ptr->p_usr.p_rts_flags)) 		
			{ret = EDVSBUSY; break;}	/* source  busy			*/
		if( caller_ptr != src_ptr) {
			if(IT_IS_REMOTE(src_ptr)){
				if(test_bit(BIT_RMTOPER,&src_ptr->p_usr.p_rts_flags))
					{ret = EDVSBUSY; break;}	/*source is enqueue in SPROXY	*/
			}else{
				if(!test_bit(BIT_RECEIVING,&src_ptr->p_usr.p_rts_flags))
					{ret = EDVSPROCSTS; break;}	/* source is not blocked receiving */
			}
		}
		if( src_ptr->p_usr.p_nodeid < 0 	
			|| src_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 	 
			{ ret = EDVSBADNODEID;break;}	
		if( ! test_bit(src_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	
			{ret = EDVSNODCNODE ;break;}	
		if( src_ptr != caller_ptr ) {
			if( test_bit(BIT_MIGRATE, &src_ptr->p_usr.p_rts_flags))	{
				set_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
				caller_ptr->p_usr.p_waitmigr = src_ep;
				INIT_LIST_HEAD(&caller_ptr->p_mlink);
				LIST_ADD_TAIL(&caller_ptr->p_mlink, &src_ptr->p_mlist);
				if( caller_ptr == dst_ptr) 
					sleep_proc2(caller_ptr, src_ptr, TIMEOUT_MOLCALL);
				else
					sleep_proc3(caller_ptr, src_ptr, dst_ptr, TIMEOUT_MOLCALL);				
				ret = caller_ptr->p_rcode;
				retry = 1;
				continue;
			}
		} 

		/*--------------------------------------------------------------*/
		/*			check DESTINATION									*/
		/*--------------------------------------------------------------*/
		if(dst_ptr->p_usr.p_endpoint != dst_ep) 			
			{ret = EDVSENDPOINT; break;} 	/* Paranoid checking		*/
		if(test_bit(BIT_SLOT_FREE, &dst_ptr->p_usr.p_rts_flags))	
			{ret = EDVSDSTDIED; break;} 	/* destination died			*/
		if(test_bit(BIT_ONCOPY, & dst_ptr->p_usr.p_rts_flags)) 		
			{ret = EDVSBUSY; break;}	/* destination busy			*/
		if( caller_ptr != dst_ptr) {
			if(IT_IS_REMOTE(dst_ptr)){
				if(test_bit(BIT_RMTOPER,&dst_ptr->p_usr.p_rts_flags)) 	
					{ret = EDVSBUSY; break;} /* destination is enqueue in SPROXY	*/
			}else{
				if(!test_bit(BIT_RECEIVING,&dst_ptr->p_usr.p_rts_flags))
					{ret = EDVSPROCSTS; break;} /* destination is not blocked receiving */
			}
		}
		if( dst_ptr->p_usr.p_nodeid < 0 	
			|| dst_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 	 
			{ ret = EDVSBADNODEID;break;}	
		if( ! test_bit(dst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	
			{ret = EDVSNODCNODE ;break;}
		if( dst_ptr != caller_ptr ) {
			if( test_bit(BIT_MIGRATE, &dst_ptr->p_usr.p_rts_flags))	{
				set_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
				caller_ptr->p_usr.p_waitmigr = dst_ep;
				INIT_LIST_HEAD(&caller_ptr->p_mlink);
				LIST_ADD_TAIL(&caller_ptr->p_mlink, &dst_ptr->p_mlist);
				if( caller_ptr == src_ptr) 
					sleep_proc2(caller_ptr, dst_ptr, TIMEOUT_MOLCALL);
				else
					sleep_proc3(caller_ptr, dst_ptr, src_ptr, TIMEOUT_MOLCALL);				
				ret = caller_ptr->p_rcode;
				retry = 1;
				continue;
			}
		}
	} while(retry);
	RUNLOCK_DC(dc_ptr);
	if(ret < 0) {	
		if( src_ptr != caller_ptr && dst_ptr != caller_ptr ) {
			WUNLOCK_PROC3(caller_ptr,src_ptr,dst_ptr);
		}else {
			if( dst_ptr != caller_ptr) {
				WUNLOCK_PROC2(caller_ptr,dst_ptr);
			}else{
				WUNLOCK_PROC2(caller_ptr,src_ptr);
			}
		}
		ERROR_RETURN(ret);
	}

	caller_nr   = caller_ptr->p_usr.p_nr;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	DVKDEBUG(DBGPARAMS,"CALLER dcid=%d caller_pid=%d caller_nr=%d caller_ep=%d \n",
		caller_ptr->p_usr.p_dcid,caller_pid,caller_nr,caller_ep);
		
	set_bit(BIT_ONCOPY, &caller_ptr->p_usr.p_rts_flags);/* to signal other process that this is busy */
	set_bit(BIT_ONCOPY, &src_ptr->p_usr.p_rts_flags); 	/* to signal other process that this is busy */
	set_bit(BIT_ONCOPY, &dst_ptr->p_usr.p_rts_flags); 	/* to signal other process that this is busy */

	DVKDEBUG(DBGPARAMS,"CALLER p_endpoint=%d \n", caller_ptr->p_usr.p_endpoint);
	DVKDEBUG(DBGPARAMS,"SOURCE p_endpoint=%d \n", src_ptr->p_usr.p_endpoint);
	DVKDEBUG(DBGPARAMS,"DESTIN p_endpoint=%d \n", dst_ptr->p_usr.p_endpoint);
	DVKDEBUG(DBGPARAMS,"BYTES  bytes	=%d \n", bytes);

	caller_ptr->p_rcode = ret =  OK;
    
	if( (IT_IS_LOCAL(src_ptr)) && (IT_IS_LOCAL(dst_ptr))) { 	/* source=LOCAL and destination=LOCAL 	*/
		src_pid  = task_pid_nr(src_ptr->p_task);
		src_tgid = src_ptr->p_task->tgid;
		dst_pid  = task_pid_nr(dst_ptr->p_task);
		dst_tgid = dst_ptr->p_task->tgid;
		DVKDEBUG(DBGPARAMS,"src_pid=%d src_tgid=%d dst_pid=%d dst_tgid=%d\n", 
			src_pid, src_tgid, dst_pid, dst_tgid);
		
//		if( dst_tgid == src_tgid){ 
		if( src_pid == dst_pid){ 
			ret = EDVSPERM;
		}else{
			while( bytes > 0) {
				copylen = (bytes > dvs.d_max_copybuf)?dvs.d_max_copybuf:bytes;
				DVKDEBUG(INTERNAL,"COPY_USR2USR_PROC copylen=%d\n", copylen);
				COPY_USR2USR_PROC(ret, NONE, src_ptr,  (char *)src_addr, dst_ptr, (char *) dst_addr, copylen);
				if(ret < 0) break;
				bytes -=copylen;
				src_addr += copylen;
				dst_addr += copylen;
			}
		}
		caller_ptr->p_rcode= ret;
	} else if( (IT_IS_LOCAL(src_ptr)) && (IT_IS_REMOTE(dst_ptr))) {/* source=LOCAL and destination=REMOTE */
		sproxy_ptr = NODE2SPROXY(dst_ptr->p_usr.p_nodeid); 
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
		if(ret == OK) {
			while( bytes > 0) {
				copylen = (bytes > NODE2MAXBYTES(dst_ptr->p_usr.p_nodeid))?
							NODE2MAXBYTES(dst_ptr->p_usr.p_nodeid):bytes;
				DVKDEBUG(INTERNAL,"CMD_COPYIN_DATA copylen=%d\n", copylen);

				/* Header of the message */
				caller_ptr->p_rmtcmd.c_cmd   = CMD_COPYIN_DATA;
				caller_ptr->p_rmtcmd.c_dcid  = caller_ptr->p_usr.p_dcid;
				caller_ptr->p_rmtcmd.c_src   = caller_ptr->p_usr.p_endpoint;	
				caller_ptr->p_rmtcmd.c_dst   = dst_ptr->p_usr.p_endpoint;	
				caller_ptr->p_rmtcmd.c_snode = atomic_read(&local_nodeid); 
				caller_ptr->p_rmtcmd.c_dnode = dst_ptr->p_usr.p_nodeid;		
				caller_ptr->p_rmtcmd.c_rcode = OK;
				caller_ptr->p_rmtcmd.c_len   = copylen;

				/* subheader for the vcopy operation */
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_src   = src_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_dst   = dst_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_rqtr  = caller_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_saddr = src_addr;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_daddr = dst_addr;	
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_bytes = copylen;	

				set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);

				cmd_ptr = &caller_ptr->p_rmtcmd;
				DVKDEBUG(DBGCMD,"CMD_COPYIN_DATA " CMD_FORMAT, CMD_FIELDS(cmd_ptr));
				DVKDEBUG(DBGVCOPY,"CMD_COPYIN_DATA " VCOPY_FORMAT, VCOPY_FIELDS(cmd_ptr));

				ret = sproxy_enqueue(caller_ptr);
				if(ret < 0) break;

				if(src_ptr != caller_ptr)
					sleep_proc3(caller_ptr, src_ptr, dst_ptr,TIMEOUT_MOLCALL);
				else
					sleep_proc2(caller_ptr, dst_ptr,TIMEOUT_MOLCALL);
				ret = caller_ptr->p_rcode;
				if(ret < 0) break;
				bytes -=copylen;
				src_addr += copylen;
				dst_addr += copylen;
			}
			
		}
	} else if( (IT_IS_REMOTE(src_ptr)) && (IT_IS_LOCAL(dst_ptr))){ /* source=REMOTE and destination=LOCAL */
		sproxy_ptr = NODE2SPROXY(src_ptr->p_usr.p_nodeid); 
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
		if(ret == OK) {
			while( bytes > 0) {
				copylen = (bytes > NODE2MAXBYTES(src_ptr->p_usr.p_nodeid))?
							NODE2MAXBYTES(src_ptr->p_usr.p_nodeid):bytes;
			
				/* Header of the command */
				caller_ptr->p_rmtcmd.c_cmd   = CMD_COPYOUT_RQST;
				caller_ptr->p_rmtcmd.c_dcid  = caller_ptr->p_usr.p_dcid;
				caller_ptr->p_rmtcmd.c_src   = caller_ptr->p_usr.p_endpoint;	
				caller_ptr->p_rmtcmd.c_dst   = src_ptr->p_usr.p_endpoint;	
				caller_ptr->p_rmtcmd.c_snode = atomic_read(&local_nodeid); 
				caller_ptr->p_rmtcmd.c_dnode = src_ptr->p_usr.p_nodeid;		
				caller_ptr->p_rmtcmd.c_rcode = OK;
				caller_ptr->p_rmtcmd.c_len   = 0;

				/* subheader for the vcopy operation */
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_src =  src_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_dst =  dst_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_rqtr=  caller_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_saddr = src_addr;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_daddr = dst_addr;	
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_bytes = copylen;

				set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);

				cmd_ptr = &caller_ptr->p_rmtcmd;
				DVKDEBUG(DBGCMD,"CMD_COPYOUT_RQST " CMD_FORMAT, CMD_FIELDS(cmd_ptr));
				DVKDEBUG(DBGVCOPY,"CMD_COPYOUT_RQST " VCOPY_FORMAT, VCOPY_FIELDS(cmd_ptr));

				ret =  sproxy_enqueue(caller_ptr);	
				if(ret < 0) break;

				if(dst_ptr != caller_ptr)
					sleep_proc3(caller_ptr, src_ptr, dst_ptr,TIMEOUT_MOLCALL);
				else
					sleep_proc2(caller_ptr, src_ptr,TIMEOUT_MOLCALL);
				ret = caller_ptr->p_rcode;
				if(ret < 0) break;
				bytes -=copylen;
				src_addr += copylen;
				dst_addr += copylen;
			}				
		}
	} else {						/* source=REMOTE and destination=REMOTE	*/
		DVKDEBUG(GENERIC,"source=REMOTE and destination=REMOTE\n");
		/* get source nodeid */
		DVKDEBUG(GENERIC,"src_ptr->p_usr.p_nodeid=%d\n", src_ptr->p_usr.p_nodeid);
		sproxy_ptr = NODE2SPROXY(src_ptr->p_usr.p_nodeid); 
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
		if( ret == OK) {
			DVKDEBUG(GENERIC,"dst_ptr->p_usr.p_nodeid=%d\n", dst_ptr->p_usr.p_nodeid);
			sproxy_ptr = NODE2SPROXY(dst_ptr->p_usr.p_nodeid); 
			RLOCK_PROC(sproxy_ptr);
			do {
				ret = OK;
				if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)			
					{ret = EDVSNOPROXY ;break;}
				if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
					{ret = EDVSNOPROXY;break;} 
				if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
					{ret = EDVSNOPROXY;break;} 
				if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
					{ret = EDVSNOTCONN;break;}  
			} while(0);
			RUNLOCK_PROC(sproxy_ptr);
		}
		
		if(ret == OK) {		
			while( bytes > 0) {
				copylen = (bytes > dvs.d_max_copybuf)?dvs.d_max_copybuf:bytes;
				if(src_ptr->p_usr.p_nodeid == dst_ptr->p_usr.p_nodeid) {	/* A remote copy on the same REMOTE node 	*/ 
					DVKDEBUG(INTERNAL,"CMD_COPYLCL_RQST copylen=%d\n", copylen);
					caller_ptr->p_rmtcmd.c_cmd   = CMD_COPYLCL_RQST;
					caller_ptr->p_rmtcmd.c_dcid  = caller_ptr->p_usr.p_dcid;
					caller_ptr->p_rmtcmd.c_src   = caller_ptr->p_usr.p_endpoint;	
					caller_ptr->p_rmtcmd.c_dst   = dst_ptr->p_usr.p_endpoint;	/* Destination endpoint 	*/
					caller_ptr->p_rmtcmd.c_snode = atomic_read(&local_nodeid); 
					caller_ptr->p_rmtcmd.c_dnode = dst_ptr->p_usr.p_nodeid;		/* destination node		*/
					caller_ptr->p_rmtcmd.c_rcode = OK;
					caller_ptr->p_rmtcmd.c_len   = 0;
				} else {													/* A remote copy on different nodes */ 
					DVKDEBUG(INTERNAL,"CMD_COPYRMT_RQST copylen=%d\n", copylen);
					caller_ptr->p_rmtcmd.c_cmd   = CMD_COPYRMT_RQST;
					caller_ptr->p_rmtcmd.c_dcid  = caller_ptr->p_usr.p_dcid;
					caller_ptr->p_rmtcmd.c_src   = caller_ptr->p_usr.p_endpoint;	
					caller_ptr->p_rmtcmd.c_dst   = src_ptr->p_usr.p_endpoint;	/* Source endpoint 	*/
					caller_ptr->p_rmtcmd.c_snode = atomic_read(&local_nodeid); 
					caller_ptr->p_rmtcmd.c_dnode = src_ptr->p_usr.p_nodeid;		/* source   node		*/
					caller_ptr->p_rmtcmd.c_rcode = OK;
					caller_ptr->p_rmtcmd.c_len   = 0;
				}
				set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
				/* subheader for the vcopy operation */
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_src =  src_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_dst =  dst_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_rqtr=  caller_ptr->p_usr.p_endpoint;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_saddr = src_addr;		
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_daddr = dst_addr;	
				caller_ptr->p_rmtcmd.c_u.cu_vcopy.v_bytes = copylen;	

				ret = sproxy_enqueue(caller_ptr);
				if(ret < 0) break;
				sleep_proc3(caller_ptr, src_ptr, dst_ptr,TIMEOUT_MOLCALL);
				ret = caller_ptr->p_rcode;
				if(ret < 0) break;
				bytes -=copylen;
				src_addr += copylen;
				dst_addr += copylen;
			}				
		}
	}

/*
* PROBLEMAS A RESOLVER: La copia de bloques de datos que involucra a nodos remotos debe cumplir con dos condiciones:
* 	Sincronizacion: si hay una copia remota pendiente se debe estar 
*   			seguro de que la misma finalizo para continuar con otra operacion
*	Paralelismo: Si el requester es un TERCERO (por ejemplo la SYSTASK) que 	
*			tiene q esperar que finalicen las operaciones de COPY pendientes, 
*			todas aquellas operaciones locales se verian retrasadas.  
* 	Soluci�n: Crear un KERNEL Thread encargado de realizar las copias remotas y de responderle al requester cuando 
*			la copia finaliz�. 
* 			
*/
	if( ret == OK)
		ret = caller_ptr->p_rcode;
	else {
		clear_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_proxy = NONE;
		caller_ptr->p_rcode = ret;
	}

	/* May be that during the copy, the SOURCE and DESTIONATION (!= REQUESTER) that were 
	 * sleeping by other reason (RECEIVING) received or sent a message, but 
	 * because the p_rts_flags &ONCOPY they were not waked up by send(), receive(), sendrec() or notify().
	 * Now, the COPY has finished, then they must be waked up.
	 */

	clear_bit(BIT_ONCOPY, &src_ptr->p_usr.p_rts_flags);
	clear_bit(BIT_ONCOPY, &dst_ptr->p_usr.p_rts_flags);
	clear_bit(BIT_ONCOPY, &caller_ptr->p_usr.p_rts_flags);

	if( caller_ptr == src_ptr){
		if(IT_IS_REMOTE(dst_ptr)){
			caller_ptr->p_usr.p_rmtcopy++;
		}else{
			caller_ptr->p_usr.p_lclcopy++;			
		}
	}
	
	if( src_ptr != caller_ptr && dst_ptr != caller_ptr ) {
		if(src_ptr->p_usr.p_rts_flags == 0)
			LOCAL_PROC_UP(src_ptr, ret);
		if(dst_ptr->p_usr.p_rts_flags == 0)
			LOCAL_PROC_UP(dst_ptr, ret);
		WUNLOCK_PROC2(src_ptr,dst_ptr);	
	}else if( dst_ptr != caller_ptr) {
		if(dst_ptr->p_usr.p_rts_flags == 0)
			LOCAL_PROC_UP(dst_ptr, ret);
		WUNLOCK_PROC(dst_ptr);
	}else{ /* src_ptr != caller_ptr */
		if(src_ptr->p_usr.p_rts_flags == 0)
			LOCAL_PROC_UP(src_ptr, ret);
		WUNLOCK_PROC(src_ptr);
	}

	WUNLOCK_PROC(caller_ptr);
	
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}

/*--------------------------------------------------------------*/
/*			new_mini_rcvrqst			*/
/* Receives a message from another MOL process of the same DC	*/
/* Differences with RECEIVE:					*/
/* 	The requester process must do sendrec => p_rts_flags =	*/
/*					(SENDING | RECEIVING)	*/
/*	The request can be ANY process 				*/
/*--------------------------------------------------------------*/
asmlinkage long new_mini_rcvrqst(message* m_ptr, long timeout_ms)
{
	struct proc *caller_ptr, *xpp, *tmp_ptr;
	dc_desc_t *dc_ptr;
	int dcid;
	int ret;
	int caller_nr, caller_pid, caller_ep;
	struct task_struct *task_ptr;	

	if( DVS_NOT_INIT() ) 				ERROR_RETURN(EDVSDVSINIT);

	DVKDEBUG(DBGPARAMS,"\n");

	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret < 0) ERROR_RETURN(ret);
	
	/*------------------------------------------
	 * Checks the caller process PID
 	 * Gets the DCID
	 * Checks if the DC is RUNNING
	 * Locks the DC
	*------------------------------------------*/
	RLOCK_PROC(caller_ptr);
	caller_nr   = caller_ptr->p_usr.p_nr;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	DVKDEBUG(DBGPARAMS,"caller_nr=%d caller_ep=%d \n", 
		caller_nr, caller_ep);
//	if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//			ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADPID);
	dcid = caller_ptr->p_usr.p_dcid;
	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
	if( dcid < 0 || dcid >= dvs.d_nr_dcs) 			
		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADDCID);
	dc_ptr 	= &dc[dcid];
	RUNLOCK_PROC(caller_ptr);

	RLOCK_DC(dc_ptr);
	if( dc_ptr->dc_usr.dc_flags)  
		ERROR_WUNLOCK_DC(dc_ptr, EDVSDCNOTRUN);
	RUNLOCK_DC(dc_ptr);	

	WLOCK_PROC(caller_ptr);
	/*--------------------------------------*/
	/* WAKE UP CONTROL 		*/
	/*--------------------------------------*/
	if( test_bit(MIS_BIT_WOKENUP, &caller_ptr->p_usr.p_misc_flags)) {
		clear_bit(MIS_BIT_WOKENUP, &caller_ptr->p_usr.p_misc_flags);
		WUNLOCK_PROC(caller_ptr);
		ERROR_RETURN(EDVSWOKENUP);
	}

	/*-----------------------------------------*/
	/* MESSAGE RECEIVE 		*/
	/*-----------------------------------------*/
	caller_ptr->p_umsg 	= m_ptr;
	
	LIST_FOR_EACH_ENTRY_SAFE(xpp, tmp_ptr, &caller_ptr->p_list, p_link) {
		if( caller_ptr->p_usr.p_nr < xpp->p_usr.p_nr) {
			WLOCK_PROC(xpp);
		}else{
			WUNLOCK_PROC(caller_ptr);
			WLOCK_PROC(xpp);
			WLOCK_PROC(caller_ptr);
		}
		DVKDEBUG(GENERIC,"Found acceptable message from %d. Copy it and update status.\n"
				,xpp->p_usr.p_endpoint );
		/* Here is a message from xpp process, therefore xpp  must be sleeping in SENDING state */

		LIST_DEL(&xpp->p_link); /* remove from queue */

		/* test the sender status */
		do	{
			if( test_bit(BIT_SLOT_FREE, &xpp->p_usr.p_rts_flags))	
				{ret = EDVSSRCDIED; break;} 	
			if( !test_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags))	
				{ret = EDVSPROCSTS; break;} 	
			if( xpp->p_usr.p_sendto != caller_ptr->p_usr.p_endpoint) 	
				{ret = EDVSPROCSTS; break;} 
			ret = OK;
		} while(0);
		
		if(ret == OK) {			
			if( IT_IS_REMOTE(xpp) ){ 
				COPY_TO_USER_PROC(ret, (void *)&xpp->p_message, 
					(void *) m_ptr, sizeof(message));
				if( !test_bit(BIT_RECEIVING, &xpp->p_usr.p_rts_flags)) {
					send_ack_lcl2rmt(xpp, caller_ptr, ret);
				}
			}else {
				COPY_USR2USR_PROC(ret, xpp->p_usr.p_endpoint, xpp, (char *)xpp->p_umsg,
					caller_ptr, (char *)m_ptr, sizeof(message) );
			} 
			clear_bit(BIT_SENDING, &xpp->p_usr.p_rts_flags);
			xpp->p_usr.p_sendto 	= NONE;
			if(xpp->p_usr.p_rts_flags == 0) 
				LOCAL_PROC_UP(xpp, ret);
		}
				
		WUNLOCK_PROC(xpp);
		WUNLOCK_PROC(caller_ptr);
		if( ret < 0) ERROR_RETURN(ret);
		return(OK);
	}
	
	set_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
	clear_bit(MIS_BIT_NOTIFY, &caller_ptr->p_usr.p_misc_flags);
	caller_ptr->p_usr.p_getfrom = ANY;
	caller_ptr->p_rcode			= OK;
	DVKDEBUG(GENERIC,"Any suitable message from was not found.\n");	

	sleep_proc(caller_ptr, timeout_ms);

	clear_bit(BIT_RECEIVING, &caller_ptr->p_usr.p_rts_flags);
	caller_ptr->p_usr.p_getfrom 	= NONE;
	ret = caller_ptr->p_rcode;

	WUNLOCK_PROC(caller_ptr);
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}	
	

/*--------------------------------------------------------------*/
/*			new_mini_reply				   */
/*--------------------------------------------------------------*/
asmlinkage long new_mini_reply(int dst_ep, message* m_ptr, long timeout_ms)
{
	struct proc *dst_ptr, *caller_ptr, *sproxy_ptr;
	dc_desc_t *dc_ptr;
	int ret, retry;
	int caller_nr, caller_ep, caller_pid;
	int dst_nr, dcid;
	struct task_struct *task_ptr;

	if( DVS_NOT_INIT() )
		ERROR_RETURN(EDVSDVSINIT);

	DVKDEBUG(DBGPARAMS,"dst_ep=%d\n",dst_ep);
	
	ret = check_caller(&task_ptr, &caller_ptr, &caller_pid);
	if(ret < 0) ERROR_RETURN(ret);

	/*------------------------------------------
	 * Checks the caller process PID
 	 * Gets the DCID
	 * Checks if the DC is RUNNING
      *------------------------------------------*/
 	RLOCK_PROC(caller_ptr);
	caller_nr   = caller_ptr->p_usr.p_nr;
	caller_ep   = caller_ptr->p_usr.p_endpoint;
	dcid		= caller_ptr->p_usr.p_dcid;
	DVKDEBUG(DBGPARAMS,"caller_nr=%d caller_ep=%d dst_ep=%d \n", caller_nr, caller_ep, dst_ep);
//	if (caller_pid != caller_ptr->p_usr.p_lpid) 	
//		ERROR_RUNLOCK_PROC(caller_ptr,EDVSBADPID);
 	RUNLOCK_PROC(caller_ptr);

	DVKDEBUG(INTERNAL,"dcid=%d\n", dcid);
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
	dst_nr = _ENDPOINT_P(dst_ep);
	if( dst_nr < (-dc_ptr->dc_usr.dc_nr_tasks)		
	 || dst_nr >= dc_ptr->dc_usr.dc_nr_procs)	
			ERROR_RETURN(EDVSRANGE)
	dst_ptr   = NBR2PTR(dc_ptr, dst_nr);
	if( dst_ptr == NULL) 				
		ERROR_RETURN(EDVSDSTDIED);
	if( caller_ep == dst_ep) 			
		ERROR_RETURN(EDVSENDPOINT);
	
 	WLOCK_PROC(caller_ptr);
	caller_ptr->p_umsg	= m_ptr;

reply_replay: /* Return point for a migrated destination process */
	WLOCK_ORDERED2(caller_ptr,dst_ptr);
	/*------------------------------------------
	 * check the destination process status
	*------------------------------------------*/
	DVKDEBUG(DBGPARAMS,"dst_nr=%d dst_ep=%d\n",dst_nr, dst_ptr->p_usr.p_endpoint);
	do	{
		ret = OK;
		retry = 0;
		if (dst_ptr->p_usr.p_endpoint != dst_ep) 	
			{ret = EDVSENDPOINT; break;} 	/* Paranoid checking		*/
		if( test_bit(BIT_SLOT_FREE, &dst_ptr->p_usr.p_rts_flags))	
			{ret = EDVSDSTDIED; break;} 	/*destination died		*/	
		if( test_bit(BIT_MIGRATE, &dst_ptr->p_usr.p_rts_flags))	{
			set_bit(BIT_WAITMIGR, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_waitmigr = dst_ep;
			INIT_LIST_HEAD(&caller_ptr->p_mlink);
			LIST_ADD_TAIL(&caller_ptr->p_mlink, &dst_ptr->p_mlist);
			sleep_proc2(caller_ptr, dst_ptr, timeout_ms);
			ret = caller_ptr->p_rcode;
			retry = 1;
			continue;
		} 	
		/* get destination nodeid */
		DVKDEBUG(INTERNAL,"dst_ptr->p_usr.p_nodeid=%d\n",dst_ptr->p_usr.p_nodeid);
		if( dst_ptr->p_usr.p_nodeid < 0 
			|| dst_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 
			{ret = EDVSBADNODEID; break;}	
		RLOCK_DC(dc_ptr);
		if( !test_bit(dst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes)) 	{
			ret = EDVSNODCNODE;
		}
		RUNLOCK_DC(dc_ptr);
	} while(retry);
	if(ret < 0) {							
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(ret);
	}
	
	// Set the message source endpoint 
	ret = put_user(caller_ep, &m_ptr->m_source);
	if( ret != OK) {							
		WUNLOCK_PROC2(caller_ptr, dst_ptr);
		ERROR_RETURN(ret);
	}
	
	DVKDEBUG(DBGPARAMS,"dcid=%d caller_pid=%d caller_nr=%d dst_ep=%d \n",
		caller_ptr->p_usr.p_dcid, caller_pid,caller_nr, dst_ep);
		
	caller_ptr->p_rcode= OK;
	if( IT_IS_REMOTE(dst_ptr)) {		/* the destination is REMOTE */
		/*---------------------------------------------------------------------------------------------------*/
		/*						REMOTE 								 */
		/*---------------------------------------------------------------------------------------------------*/
		sproxy_ptr = NODE2SPROXY(dst_ptr->p_usr.p_nodeid);
		RLOCK_PROC(sproxy_ptr);
		do {
			ret = OK;
			if( sproxy_ptr->p_usr.p_lpid == PROC_NO_PID)		
				{ret = EDVSNOPROXY ;break;}
			if( test_bit( BIT_SLOT_FREE, &sproxy_ptr->p_usr.p_rts_flags))	
				{ret = EDVSNOPROXY ;break;} 
			if( !test_bit(MIS_BIT_PROXY, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOPROXY;break;} 
			if( !test_bit(MIS_BIT_CONNECTED, &sproxy_ptr->p_usr.p_misc_flags))	
				{ret = EDVSNOTCONN;break;}  
		} while(0);
		RUNLOCK_PROC(sproxy_ptr);
 		if(ret < 0) {							
			WUNLOCK_PROC2(caller_ptr, dst_ptr);
			ERROR_RETURN(ret);
		}

		clear_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags);
		if(	dst_ptr->p_usr.p_getfrom == caller_ptr->p_usr.p_endpoint){
			dst_ptr->p_usr.p_getfrom = NONE;
		}

		/* fill the caller's rmtcmd fields */
		caller_ptr->p_rmtcmd.c_cmd 		= CMD_REPLY_MSG;
		caller_ptr->p_rmtcmd.c_src 		= caller_ep;
		caller_ptr->p_rmtcmd.c_dst 		= dst_ep;
		caller_ptr->p_rmtcmd.c_dcid		= caller_ptr->p_usr.p_dcid;
		caller_ptr->p_rmtcmd.c_snode  	= atomic_read(&local_nodeid);
		caller_ptr->p_rmtcmd.c_dnode  	= dst_ptr->p_usr.p_nodeid;
		caller_ptr->p_rmtcmd.c_rcode  	= OK;
		caller_ptr->p_rmtcmd.c_len  	= 0;
		WUNLOCK_PROC(dst_ptr); 
		COPY_FROM_USER_PROC(ret, (char*) &caller_ptr->p_rmtcmd.c_u.cu_msg, 
			m_ptr, sizeof(message) );
			
		INIT_LIST_HEAD(&caller_ptr->p_link);
		set_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
		caller_ptr->p_usr.p_sendto = dst_ep;
		set_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
		
		ret = sproxy_enqueue(caller_ptr);
			
		/* wait for the REPLY_ACK */
		sleep_proc(caller_ptr, timeout_ms);
		ret = caller_ptr->p_rcode;
		if( ret == OK){
			caller_ptr->p_usr.p_rmtsent++;
		}else{
			if( test_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags)) {
				DVKDEBUG(GENERIC,"removing %d link from sender's proxy list.\n", 
					caller_ptr->p_usr.p_endpoint);
				LIST_DEL(&caller_ptr->p_link); /* remove from queue ATENCION: HAY Q PROTEGER PROXY ?? */
			}
			clear_bit(BIT_SENDING, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_sendto = NONE;
			clear_bit(BIT_RMTOPER, &caller_ptr->p_usr.p_rts_flags);
			caller_ptr->p_usr.p_proxy = NONE;
			if( ret == EDVSMIGRATE) {
				WUNLOCK_PROC(caller_ptr); 
				goto reply_replay;
			}
		}
	} else {					/* the destination is LOCAL  */
		/*---------------------------------------------------------------------------------------------------*/
		/*						LOCAL								 */
		/*---------------------------------------------------------------------------------------------------*/
		/* Check if 'dst' is blocked waiting for this message.   */
		if ( (test_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags) && 
				!test_bit(BIT_SENDING, &dst_ptr->p_usr.p_rts_flags)) &&
				(dst_ptr->p_usr.p_getfrom == caller_ep)) {
			DVKDEBUG(GENERIC,"destination is waiting. Copy the message and wakeup destination\n");

			COPY_USR2USR_PROC(ret, caller_ep, caller_ptr, (char *) m_ptr, 
				dst_ptr, (char *)dst_ptr->p_umsg, sizeof(message) );
			if(ret < 0) {
				WUNLOCK_PROC2(caller_ptr, dst_ptr);
				ERROR_RETURN(ret);
			}
			clear_bit(BIT_RECEIVING,&dst_ptr->p_usr.p_rts_flags);
			dst_ptr->p_usr.p_getfrom 	= NONE;
			if(dst_ptr->p_usr.p_rts_flags == 0) 
				LOCAL_PROC_UP(dst_ptr, ret);
			caller_ptr->p_usr.p_lclsent++;
		} else { 
			DVKDEBUG(GENERIC,"destination is not waiting dst_flags=%lX. Enqueue at the TAIL.\n"
				,dst_ptr->p_usr.p_rts_flags);
			/* The destination is not waiting for this message 			*/
			/* Append the caller at the TAIL of the destination senders' queue	*/
			/* blocked sending the message */
			ret = EDVSPROCSTS;
		}
		WUNLOCK_PROC(dst_ptr);
	}
	WUNLOCK_PROC(caller_ptr);
	if(ret < 0) ERROR_RETURN(ret);
	return(ret);
}


/*--------------------------------------------------------------*/
/*			new_hdw_notify				   */
/*--------------------------------------------------------------*/
asmlinkage long new_hdw_notify(int dcid, int dst_ep)
{
	
	struct proc *dst_ptr, *hdw_ptr;
	dc_desc_t *dc_ptr;
	int dst_nr;
	int ret;
	message *mptr;
	
	DVKDEBUG(DBGPARAMS,"dcid=%d dst_ep=%d \n", dcid, dst_ep);
#ifdef DVS_CAPABILITIES
	 if ( !capable(CAP_VOS_ADMIN)) ERROR_RETURN(EDVSPRIVILEGES);
#else
	if(get_current_cred()->euid.val != USER_ROOT) ERROR_RETURN(EDVSPRIVILEGES);
#endif 

	if( DVS_NOT_INIT() ) 			ERROR_RETURN(EDVSDVSINIT);

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
	dst_nr = _ENDPOINT_P(dst_ep);
	if( dst_nr < (-dc_ptr->dc_usr.dc_nr_tasks)		
	 || dst_nr >= dc_ptr->dc_usr.dc_nr_procs)	
		ERROR_RETURN(EDVSRANGE)
	dst_ptr   = NBR2PTR(dc_ptr, dst_nr);
	if( dst_ptr == NULL) 				
		ERROR_RETURN(EDVSDSTDIED);
	if( HARDWARE == dst_ep) 			
		ERROR_RETURN(EDVSENDPOINT);

	WLOCK_PROC(dst_ptr);
	if( test_bit(BIT_SLOT_FREE, &dst_ptr->p_usr.p_rts_flags))
		ERROR_WUNLOCK_PROC(dst_ptr, EDVSDSTDIED);
		
	if( dst_ptr->p_usr.p_nodeid < 0 
		|| dst_ptr->p_usr.p_nodeid >= dvs.d_nr_nodes) 	 
		ERROR_WUNLOCK_PROC(dst_ptr, EDVSBADNODEID);
	
	if( ! test_bit(dst_ptr->p_usr.p_nodeid, &dc_ptr->dc_usr.dc_nodes))
		ERROR_WUNLOCK_PROC(dst_ptr, EDVSNODCNODE);
	
	if( test_bit(BIT_MIGRATE, &dst_ptr->p_usr.p_rts_flags)) 	/*destination is migrating	*/
		ERROR_WUNLOCK_PROC(dst_ptr, EDVSMIGRATE);
	
	if( dst_ptr->p_priv.priv_usr.priv_id >= dc_ptr->dc_usr.dc_nr_sysprocs) 
		ERROR_WUNLOCK_PROC(dst_ptr, EDVSPRIVILEGES);

	if( IT_IS_REMOTE(dst_ptr)) 		/* the destination is REMOTE */
		ERROR_WUNLOCK_PROC(dst_ptr, EDVSRMTPROC);	
		
	hdw_ptr = NBR2PTR(dc_ptr, HARDWARE);
	RLOCK_PROC(hdw_ptr);
	/* Check if 'dst' is blocked waiting for this message.   */
	ret = OK;
	if (  (test_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags) 
		&& !test_bit(BIT_SENDING, &dst_ptr->p_usr.p_rts_flags) )&&
		(dst_ptr->p_usr.p_getfrom == ANY || dst_ptr->p_usr.p_getfrom == HARDWARE)) {
		DVKDEBUG(GENERIC,"destination is waiting. Build the message and wakeup destination\n");
		BUILD_NOTIFY_MSG(dst_ptr, dst_ep, dst_nr, dst_ptr); 
		mptr = &dst_ptr->p_message;
		DVKDEBUG(DBGMESSAGE, MSG9_FORMAT, MSG9_FIELDS(mptr));			
		set_bit(MIS_BIT_NOTIFY, &dst_ptr->p_usr.p_misc_flags);
		clear_bit(BIT_RECEIVING, &dst_ptr->p_usr.p_rts_flags);
		dst_ptr->p_usr.p_getfrom 	= NONE;
		if(dst_ptr->p_usr.p_rts_flags == 0)
			LOCAL_PROC_UP(dst_ptr, OK); /* ATENTI puede haber mas de una razon para despertar al proceso!! */
	} else {  
		DVKDEBUG(GENERIC,"destination is not waiting dst_ptr->p_usr.p_rts_flags=%lX priv_id=%d\n"
			,dst_ptr->p_usr.p_rts_flags, dst_ptr->p_priv.priv_usr.priv_id);
  		/* Add to destination the bit map with pending notifications  */
		if(get_sys_bit(dst_ptr->p_priv.priv_notify_pending, dst_ptr->p_priv.priv_usr.priv_id)){
			DVKDEBUG(GENERIC,"get_sys_bit priv_notify_pending=%X priv_id=%d\n", 
				dst_ptr->p_priv.priv_notify_pending, dst_ptr->p_priv.priv_usr.priv_id);
			ERROR_PRINT(EDVSOVERRUN);
			ret = EDVSOVERRUN;
		}
		set_sys_bit(dst_ptr->p_priv.priv_notify_pending, dst_ptr->p_priv.priv_usr.priv_id); 
		DVKDEBUG(GENERIC,"set_sys_bit priv_notify_pending=%X priv_id=%d\n", 
				dst_ptr->p_priv.priv_notify_pending, dst_ptr->p_priv.priv_usr.priv_id);
		int i;
		PRINT_MAP(dst_ptr->p_priv.priv_notify_pending);		
	}
	RUNLOCK_PROC(hdw_ptr);
	WUNLOCK_PROC(dst_ptr);
	return(ret);
}


