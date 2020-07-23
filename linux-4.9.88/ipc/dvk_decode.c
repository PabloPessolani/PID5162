
#include "dvk_ipc.h"

/*--------------------------------------------------------------*/
/*			ipc_dvs_init			*/
/* Initialize the DVS system					*/
/* du_addr = NULL => already loaded DVS parameters 	*/ 
/* returns the local_node id if OK or negative on error	*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_dvs_init(int nodeid, 
							unsigned long second,
							unsigned long third,  
							dvs_usr_t __user *du_addr,
							long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"nodeid=%d\n", nodeid);
	rcode = new_dvs_init( nodeid, du_addr);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_dc_init				*/
/* Initializa a DC and all of its processes			*/
/* returns the local_node ID					*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_dc_init(int first, 
							unsigned long second,
							unsigned long third,
							dc_usr_t __user *dcu_addr,
							long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_dc_init(dcu_addr);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_mini_send				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_send(	int dst_ep,
								unsigned long second,
								unsigned long third,
 								message __user * m_ptr, 
								long timeout_ms)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dst_ep=%d timeout_ms=%ld\n", dst_ep, timeout_ms);
	rcode = new_mini_send(dst_ep, m_ptr, timeout_ms);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_mini_receive			*/
/* Receives a message from another MOL process of the same DC	*/
/* Differences with MINIX:					*/
/*	- The receiver copies the message from sender's buffer  */
/*	   to receiver's userspace 				*/
/*	- After a the receiver is unblocked, it must check if 	*/
/*	   it was for doing a copy command (CMD_COPY_IN, CMD_COPY_OUT)	*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_receive(	int src_ep, 
									unsigned long second,
									unsigned long third,
									message __user * m_ptr, 
									long timeout_ms)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"src_ep=%d timeout_ms=%ld\n", src_ep, timeout_ms);
	rcode = new_mini_receive(src_ep, m_ptr, timeout_ms);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_mini_notify				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_notify(	int src_nr, 
									int dst_ep, 
									int update_proc,
									void __user * ptr, 
									long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"src_nr=%d dst_ep=%d update_proc=%d\n", src_nr, dst_ep, update_proc);
	rcode = new_mini_notify(src_nr, dst_ep, update_proc);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_mini_sendrec			*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_sendrec(	int srcdst_ep, 
									unsigned long second,
									unsigned long third,
									message __user * m_ptr, 
									long timeout_ms)
{ 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"srcdst_ep=%d timeout_ms=%ld\n", srcdst_ep, timeout_ms);
	rcode = new_mini_sendrec(srcdst_ep, m_ptr, timeout_ms);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_mini_rcvrqst			*/
/* Receives a message from another MOL process of the same DC	*/
/* Differences with RECEIVE:					*/
/* 	The requester process must do sendrec => p_rts_flags =	*/
/*					(SENDING | RECEIVING)	*/
/*	The request can be ANY process 				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_rcvrqst( 	int first, 
									unsigned long second,
									unsigned long third,	
									message __user * m_ptr, 
									long timeout_ms)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"timeout_ms=%ld\n", timeout_ms);
	rcode = new_mini_rcvrqst(m_ptr, timeout_ms);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_mini_reply				   */
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_reply(	int dst_ep, 
								unsigned long second,
								unsigned long third,	
								message __user * m_ptr, 
								long timeout_ms)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dst_ep=%d timeout_ms=%ld\n", dst_ep, timeout_ms);
	rcode = new_mini_reply(dst_ep, m_ptr, timeout_ms);
	return(rcode);
}

/*----------------------------------------------------------------*/
/*			ipc_dc_end				*/
/*----------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_dc_end(	int dcid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{  
	long rcode;
	DVKDEBUG(DBGPARAMS,"dcid=%d\n", dcid);
	rcode = new_dc_end( dcid);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_bind				*/
/* Binds (an Initialize) a Linux process to the IPC Kernel	*/
/* Who can call bind?:						*/
/* - The main thread of a process to bind itself (mnx_bind)	*/
/* - The child thread of a process to bind itself (mnx_tbind)	*/
/* - A DVK process that bind a local process (mnx_lclbind)	*/
/* - A DVK process that bind a remote process (mnx_rmtbind)	*/
/* - A DVK process that bind a local process that is a backup of*/
/*	a remote active process (mnx_bkupbind). Then, with	*/
/*	ipc_migrate, the backup process can be converted into   */
/*	the primary process					*/
/* -  A DVK process that bind a local REPLICATED process (mnx_repbind)	*/
/* Local process: proc = proc number				*/
/* Remote  process: proc = endpoint 				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_bind(int oper, int dcid, int param_pid, int endpoint, int nodeid)
{  
	long rcode;
	DVKDEBUG(DBGPARAMS,"oper=%d dcid=%d param_pid=%d endpoint=%d nodeid=%d\n",oper, dcid, param_pid,
						endpoint, nodeid);
	rcode = new_bind(oper, dcid, param_pid,	endpoint, nodeid);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_unbind				*/
/* Unbind a  LOCAL/REMOTE  process from the DVS */
/* Who can ipc_unbind a process?:				*/
/* - The main thread of a LOCAL process itself		*/
/* - The child thread of a LOCAL process itself		*/
/* - A system process that unbind a local/remote process	*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_unbind(	int dcid, 
							int proc_ep, 
							long timeout_ms,
							void __user * ptr, 
							long fifth) 
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d proc_ep=%d timeout_ms=%ld\n",dcid, proc_ep, timeout_ms);
	rcode = new_unbind(dcid, proc_ep, timeout_ms);
	return(rcode);
}

/*----------------------------------------------------------------*/
/*			ipc_getpriv				*/
/*----------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getpriv(	int dcid, 
								int proc_ep, 
								unsigned long third,
								priv_usr_t __user *u_priv,
								long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d proc_ep=%d\n",dcid, proc_ep);
	rcode = new_getpriv(dcid, proc_ep, u_priv);
	return(rcode);
}

/*----------------------------------------------------------------*/
/*			ipc_setpriv				*/
/*----------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_setpriv(	int dcid, 
								int proc_ep, 
								unsigned long third,
								priv_usr_t *u_priv,
								long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d proc_ep=%d\n",dcid, proc_ep);
	rcode = new_setpriv(dcid, proc_ep, u_priv);
	return(rcode);
}

long ipc_vcopy(unsigned long arg)
{  
	parm_vcopy_t vc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &vc, (const void __user *) arg, sizeof(parm_vcopy_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_vcopy(vc.v_src,  vc.v_saddr, vc.v_dst, vc.v_daddr, vc.v_bytes);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_getdcinfo				*/
/* Copies the DC entry to userspace				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getdcinfo(	int dcid,
								unsigned long second,
								unsigned long third,
								dc_usr_t *dc_usr_ptr,
								long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d\n",dcid);
	rcode = new_getdcinfo(dcid, dc_usr_ptr);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_getprocinfo				*/
/* Copies a proc descriptor to userspace			*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getprocinfo(int dcid, 
								int p_nr, 
								unsigned long third,
								struct proc_usr __user *proc_usr_ptr,
								long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d p_nr=%d\n",dcid, p_nr);
	rcode = new_getprocinfo(dcid, p_nr, proc_usr_ptr);
	return(rcode);
}


/*--------------------------------------------------------------*/
/*			ipc_mini_relay				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_mini_relay(	int dst_ep, 
								unsigned long second,
								unsigned long third,
								message __user * m_ptr,
								long fifth)
{
	return(EDVSNOSYS);
}

/*----------------------------------------------------------------*/
/*			ipc_proxies_bind			*/
/* it returns the proxies ID or ERROR				*/
/*----------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_proxies_bind(	int px_nr, 
									int spid, 
									int rpid,	
									char __user *px_name,  
									int maxbytes)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"px_nr=%d spid=%d rpid=%d px_name=%s maxbytes=%d\n", 
		px_nr, spid,rpid,px_name, maxbytes);
	rcode = new_proxies_bind(px_name, px_nr, spid, rpid, maxbytes);
	return(rcode);
}

/*----------------------------------------------------------------*/
/*			ipc_proxies_unbind			*/
/*----------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_proxies_unbind(	int px_nr,
									unsigned long second,
									unsigned long third, 
									void __user * ptr, 
									long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"px_nr=%d\n",px_nr);
	rcode = new_proxies_unbind(px_nr);
	return(rcode);
}

 /*--------------------------------------------------------------*/
/*			ipc_getnodeinfo				*/
/* Copies the NODE entry to userspace				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getnodeinfo(int nodeid, 
								unsigned long second,
								unsigned long third,
								node_usr_t *node_usr_ptr,
								 long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"nodeid=%d \n",nodeid);
	rcode = new_getnodeinfo(nodeid, node_usr_ptr);
	return(rcode);
}



/*--------------------------------------------------------------*/
/*			ipc_put2lcl				*/
/* RECEIVER proxy makes an operation to a local process requested*/
/* by a remote process						*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_put2lcl(int first,	
							proxy_hdr_t __user * usr_hdr_ptr, 
							proxy_payload_t  __user * usr_pay_ptr, 
							void __user * ptr,
							long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_put2lcl(usr_hdr_ptr, usr_pay_ptr);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_get2rmt				*/
/* proxy gets local (messages, notifies, errors, ups, data,etc  */
/* to send to a remote processes	 			*/
/* usr_hdr_ptr: buffer address in userspace for the header	*/
/* usr_pay_ptr: buffer address in userspace for the payload	*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_get2rmt(int first,
							proxy_hdr_t __user * usr_hdr_ptr, 
							proxy_payload_t  __user * usr_pay_ptr, 
							void __user * ptr,
							long timeout_ms)
{ 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"timeout_ms=%ld\n", timeout_ms);
	rcode = new_get2rmt(usr_hdr_ptr, usr_pay_ptr, timeout_ms);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_add_node				*/
/* Initializa a cluster node to be used by a DC 		*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_add_node(	int dcid, 
								int nodeid,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d nodeid=%d \n", dcid, nodeid);
	rcode = new_add_node(dcid, nodeid);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_del_node				*/
/* Delete a cluster node from a DC 		 		*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_del_node(	int dcid, 
								int nodeid,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d nodeid=%d \n", dcid, nodeid);
	rcode = new_del_node(dcid, nodeid);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_dvs_end				*/
/* End the DVS system						*/
/* - Unbind Proxies and delete Remote Nodes and its processes	*/
/* - End al DCs: unbind local process and remove commands	*/
/* - Unbind Proxies						*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_dvs_end(	int first, 
								unsigned long second,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_dvs_end();
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_getep				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getep(	int pid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{ 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"pid=%d\n",pid);
	rcode = new_getep(pid);
	return(rcode);
}
	
/*--------------------------------------------------------------*/
/*			ipc_getdvsinfo				*/
/* On return: if (ret >= 0 ) return local_nodeid 		*/
/*         and the DVS configuration  parameters 		*/
/* if ret == -1, the DVS has not been initialized		*/
/* if ret < -1, a copy_to_user error has ocurred		*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getdvsinfo(	int first, 
								unsigned long second,
								unsigned long third,
								dvs_usr_t __user *dvs_usr_ptr, 
								long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_getdvsinfo(dvs_usr_ptr);
	return(rcode);
}

/*----------------------------------------------------------------*/
/*			ipc_proxy_conn				*/
/* Its is used by the proxies to signal that that they are 	*/
/* connected/disconnected to proxies on remote nodes		*/
/* status can be: 						*/
/* CONNECT_PROXIES or DISCONNECT_PROXIES 			*/
/*----------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_proxy_conn(	int px_nr, 
								int status,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{ 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"px_nr=%d status=%X\n", px_nr, status);
	rcode = new_proxy_conn(px_nr, status);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_wait4bind										*/
/* IT waits for self process binding or other process unbining 	*/
/*  WAIT4BIND block until bind. It returns process endpoint */
/*  WAIT4BIND_T block until bind. It returns process endpoint */
/*			or (EDVSTIMEDOUT) on timed out		*/
/*  WAIT4UNBIND block until a process unbind. ret =0  */
/*  WAIT4UNBIND_T block until a process unbind	   */
/*		returning =0 or (-1) on timed out 		*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_wait4bind(int oper, 
							int other_ep, 
							long timeout_ms,
							void __user * ptr, 
							long fifth)
{ 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"oper=%d other_ep=%d timeout_ms=%ld\n", oper, other_ep, timeout_ms);

	rcode = new_wait4bind(oper, other_ep, timeout_ms);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			new_migrate				*/
/* A process can only migrate when it request a service		*/
/* to its SYSTASK (sendrec operation), therefore its p_rts_flags*/
/* must have only de BIT_RECEIVING and the p_getfrom = SYSTASK  */
/* The caller of new_migrate would be:				*/
/*	- MIGR_START: old node's SYSTASK 			*/
/*	- MIGR_COMMIT:   new node's SYSTASK 			*/
/*	- MIGR_ROLLBACK: old node's SYSTASK 			*/
/* dcid: get from caller => must be binded in the DC		*/
/* ONLY THE MAIN THREAD aka GROUP LEADER		*/
/* can be invoked in new_migrate					*/
/* All threads in the process must be in the correct		*/
/* state to be migrated							*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_migrate(	int oper, 
								int pid, 
								int dcid, 
								int endpoint, 
								int new_nodeid)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"oper=%d pid=%d dcid=%d endpoint=%d new_nodeid=%d\n",
		oper, pid, dcid, endpoint, new_nodeid);
	rcode = new_migrate(oper, pid, dcid, endpoint, new_nodeid);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_node_up				*/
/* Link a node withe a proxy pair				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_node_up(	int nodeid, 
								int px_nr, 
								unsigned long third, 
								char __user *node_name, 
								long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"nodeid=%d px_nr=%d node_name=%s\n", nodeid, px_nr, node_name);
	rcode = new_node_up(node_name, nodeid, px_nr);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_node_down				*/
/* Unlink a node from a proxy pair				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_node_down(	int nodeid,
								unsigned long second,
								unsigned long third, 
								void __user * ptr,
								long fifth)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"nodeid=%d\n", nodeid);
	rcode = new_node_down(nodeid);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_getproxyinfo				*/
/* Copies a sproxy and rproxy proc descriptor to userspace  */
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_getproxyinfo(int px_nr,  
								struct proc_usr __user *sproc_usr_ptr, 
								struct proc_usr __user *rproc_usr_ptr,
								void __user * ptr, 
								long fifth)
{
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"px_nr=%d\n", px_nr);
	rcode = new_getproxyinfo(px_nr, sproc_usr_ptr, rproc_usr_ptr);
	return(rcode);
}


/*--------------------------------------------------------------*/
/*			ipc_wakeup				*/
/*--------------------------------------------------------------*/
// int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
 long ipc_wakeup(	int dcid, 
							int proc_ep,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{

	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"dcid=%d proc_ep=%d\n", dcid, proc_ep);
	rcode = new_wakeup(dcid, proc_ep);
	return(rcode);
}

/*--------------------------------------------------------------*/
/*			ipc_exit_unbind				*/
/* Called by LINUX exit() function when a process exits */
/*--------------------------------------------------------------*/
long ipc_exit_unbind(long code)
{ 
	DVKDEBUG(DBGPARAMS,"code=%ld\n", code);
	return(code);
}

