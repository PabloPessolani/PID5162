
#ifdef  CONFIG_DVKIPC 

long mod_dvk_ipc(call, int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
{
	return((*dvk_ipc_routine[call])(first, second, third, ptr, fifth));
}

long ipc_dc_init(int first, 
							unsigned long second,
							unsigned long third,
							dc_usr_t __user *dcu_addr,
							long fifth)
{
	return(new_dc_init(dcu_addr));
}

long ipc_mini_send(	int dst_ep,
								unsigned long second,
								unsigned long third,
 								message __user * m_ptr, 
								long timeout_ms)
{
	return(new_mini_send(dst_ep, m_ptr, timeout_ms));
}

long ipc_mini_receive(	int src_ep, 
									unsigned long second,
									unsigned long third,
									message __user * m_ptr, 
									long timeout_ms)
{
	return(new_mini_receive(src_ep, m_ptr, timeout_ms));
}

long ipc_void3(void);
{
	return(EDVSNOSYS);
}

long ipc_mini_notify(	int src_nr, 
									int dst_ep, 
									int update_proc,
									void __user * ptr, 
									long fifth)
{
	return(new_mini_notify(src_nr, dst_ep, update_proc));
}
									
long ipc_mini_sendrec(	int srcdst_ep, 
									unsigned long second,
									unsigned long third,
									message __user * m_ptr, 
									long timeout_ms)
{
	return(new_mini_sendrec(srcdst_ep, m_ptr,timeout_ms));
}

long ipc_mini_rcvrqst( 	int first, 
									unsigned long second,
									unsigned long third,	
									message __user * m_ptr, 
									long timeout_ms)
{
	return(new_mini_rcvrqst(m_ptr, timeout_ms));
}

long ipc_mini_reply(	int dst_ep, 
								unsigned long second,
								unsigned long third,	
								message __user * m_ptr, 
								long timeout_ms)
{
	return(new_mini_reply(dst_ep, m_ptr, timeout_ms));
}

long ipc_dc_end(	int dcid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{
	return(new_dc_end(dcid));
}

long ipc_bind(int oper, int dcid, int param_pid, int endpoint, int nodeid)
{
	return(new_bind( oper,  dcid,  param_pid,  endpoint,  nodeid));
}

long ipc_unbind(	int dcid, 
							int proc_ep, 
							long timeout_ms,
							void __user * ptr, 
							long fifth)
{
	return(new_unbind(dcid, proc_ep, timeout_ms));
}

long ipc_dc_dump( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
{
	return(new_dc_dump());
}

long ipc_proc_dump(int dcid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{
	return(new_proc_dump(dcid));
}

long ipc_getpriv(	int dcid, 
								int proc_ep, 
								unsigned long third,
								priv_usr_t __user *u_priv,
								long fifth)
{
	return(new_getpriv(dcid, proc_ep, u_priv));
}
 long ipc_setpriv(	int dcid, 
								int proc_ep, 
								unsigned long third,
								priv_usr_t *u_priv,
								long fifth)
{
	return(new_setpriv(dcid, proc_ep, u_priv));
}

long ipc_vcopy(	int src_ep, 
							char *src_addr, 
							int dst_ep,
							char *dst_addr, 
							int bytes)
{
	return(new_vcopy( src_ep, src_addr, dst_ep, dst_addr, bytes));
}

long ipc_getdcinfo(	int dcid,
								unsigned long second,
								unsigned long third,
								dc_usr_t *dc_usr_ptr,
								long fifth)
{
	return(new_getdcinfo(dcid, dc_usr_ptr));
}

long ipc_getprocinfo(int dcid, 
								int p_nr, 
								unsigned long third,
								struct proc_usr __user *proc_usr_ptr,
								long fifth)
{
	return(new_getprocinfo( dcid,  p_nr,  proc_usr_ptr));
}

long ipc_void18( int first, unsigned long second,unsigned long third, void __user * ptr, long fifth)
{
	return(EDVSNOSYS);
}

long ipc_mini_relay(	int dst_ep, 
								unsigned long second,
								unsigned long third,
								message __user * m_ptr,
								long fifth)
{
	return(new_mini_relay(dst_ep, m_ptr));
}


long ipc_proxies_bind(	int px_nr, 
									int spid, 
									int rpid,	
									char __user *px_name,  
									int maxbytes)
{
	return(new_proxies_bind(px_name, px_nr, spid, rpid, maxbytes));
}

long ipc_proxies_unbind(	int px_nr,
									unsigned long second,
									unsigned long third, 
									void __user * ptr, 
									long fifth)
{
	return(new_proxies_unbind(px_nr));
}
									
long ipc_getnodeinfo(int nodeid, 
								unsigned long second,
								unsigned long third,
								node_usr_t *node_usr_ptr,
								 long fifth)
{
	return(new_getnodeinfo(nodeid, node_usr_ptr));
}

long ipc_put2lcl(int first,
							proxy_hdr_t __user * usr_hdr_ptr, 
							proxy_payload_t  __user * usr_pay_ptr, 
							void __user * ptr,
							long fifth)
{
	return(new_put2lcl(usr_hdr_ptr, usr_pay_ptr));
}

long ipc_get2rmt(int first,	
							proxy_hdr_t __user * usr_hdr_ptr, 
							proxy_payload_t  __user * usr_pay_ptr, 
							void __user * ptr,
							long timeout_ms)
{
	return(new_get2rmt(usr_hdr_ptr, usr_pay_ptr, timeout_ms));
}

long ipc_add_node(	int dcid, 
								int nodeid,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	return(new_add_node( dcid,  nodeid));
}

long ipc_del_node(	int dcid, 
								int nodeid,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	return(new_del_node( dcid,  nodeid));
}

long ipc_dvs_init(int nodeid, 
							unsigned long second,
							unsigned long third,  
							dvs_usr_t __user *du_addr,
							long fifth)
{
	return(new_dvs_init( nodeid, du_addr));
}

long ipc_dvs_end(	int first, 
								unsigned long second,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	return(new_dvs_end());
}

long ipc_getep(	int pid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{
	return(new_getep(pid));
}

long ipc_getdvsinfo(	int first, 
								unsigned long second,
								unsigned long third,
								dvs_usr_t __user *dvs_usr_ptr, 
								long fifth)
{
	return(new_getdvsinfo(dvs_usr_ptr));
}

long ipc_proxy_conn(	int px_nr, 
								int status,
								unsigned long third, 
								void __user * ptr, 
								long fifth)
{
	return(new_proxy_conn( px_nr, status));
}

long ipc_wait4bind(int oper, 
							int other_ep, 
							long timeout_ms,
							void __user * ptr, 
							long fifth)
{
	return(new_wait4bind( oper,  other_ep,  timeout_ms));
}

long ipc_migrate(	int oper, 
								int pid, 
								int dcid, 
								int endpoint, 
								int new_nodeid)
{
	return(new_migrate( oper,  pid,  dcid,  endpoint,  new_nodeid));
}

long ipc_node_up(	int nodeid, 
								int px_nr, 
								unsigned long third, 
								char __user *node_name, 
								long fifth)
{
	return(new_node_up(node_name, nodeid, px_nr));
}

long ipc_node_down(	int nodeid,
								unsigned long second,
								unsigned long third, 
								void __user * ptr,
								long fifth)
{
	return(new_node_down(nodeid));
}

long ipc_getproxyinfo(int px_nr,  
								struct proc_usr __user *sproc_usr_ptr, 
								struct proc_usr __user *rproc_usr_ptr,
								void __user * ptr, 
								long fifth)
{
	return(new_getproxyinfo( px_nr,   sproc_usr_ptr,  rproc_usr_ptr));
}

long ipc_wakeup(	int dcid, 
							int proc_ep,
							unsigned long third, 
							void __user * ptr, 
							long fifth)
{
	return(new_wakeup( dcid, proc_ep));
}

long ipc_exit_unbind(long code);

#endif //CONFIG_DVKIPC 
								
	
	