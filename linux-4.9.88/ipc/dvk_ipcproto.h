
 long ipc_dc_init(int first, 
							unsigned long second,
							unsigned long third,
							dc_usr_t __user *dcu_addr,
							long fifth);
 long ipc_mini_send(	int dst_ep,
								unsigned long second,
								unsigned long third,
 								message __user * m_ptr, 
								long timeout_ms);
 long ipc_mini_receive(	int src_ep, 
									unsigned long second,
									unsigned long third,
									message __user * m_ptr, 
									long timeout_ms);
 long ipc_void3(void);
 long ipc_mini_notify(	int src_nr, 
									int dst_ep, 
									int update_proc,
									void __user * ptr, 
									long fifth);
 long ipc_mini_sendrec(	int srcdst_ep, 
									unsigned long second,
									unsigned long third,
									message __user * m_ptr, 
									long timeout_ms);
 long ipc_mini_rcvrqst( 	int first, 
									unsigned long second,
									unsigned long third,	
									message __user * m_ptr, 
									long timeout_ms);
 long ipc_mini_reply(	int dst_ep, 
								unsigned long second,
								unsigned long third,	
								message __user * m_ptr, 
								long timeout_ms);
 long ipc_dc_end(	int dcid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth);
 long ipc_bind(int oper, int dcid, int param_pid, int endpoint, int nodeid);
 long ipc_unbind(	int dcid, 
							int proc_ep, 
							long timeout_ms,
							void __user * ptr, 
							long fifth);
 long ipc_dc_dump(int dcid);
 long ipc_proc_dump(int dcid);
 long ipc_getpriv(	int dcid, 
								int proc_ep, 
								unsigned long third,
								priv_usr_t __user *u_priv,
								long fifth);
 long ipc_setpriv(	int dcid, 
								int proc_ep, 
								unsigned long third,
								priv_usr_t *u_priv,
								long fifth);
 long ipc_vcopy(	int src_ep, 
							char *src_addr, 
							int dst_ep,
							char *dst_addr, 
							int bytes);
 long ipc_getdcinfo(	int dcid,
								unsigned long second,
								unsigned long third,
								dc_usr_t *dc_usr_ptr,
								long fifth);
 long ipc_getprocinfo(int dcid, 
								int p_nr, 
								unsigned long third,
								struct proc_usr __user *proc_usr_ptr,
								long fifth);
 long ipc_void18(void);
 long ipc_mini_relay(	int dst_ep, 
								unsigned long second,
								unsigned long third,
								message __user * m_ptr,
								long fifth);
 long ipc_proxies_bind(	int px_nr, 
									int spid, 
									int rpid,	
									char __user *px_name,  
									int maxbytes);
 long ipc_proxies_unbind(	int px_nr,
									unsigned long second,
									unsigned long third, 
									void __user * ptr, 
									long fifth);
 long ipc_getnodeinfo(int nodeid, 
								unsigned long second,
								unsigned long third,
								node_usr_t *node_usr_ptr,
								 long fifth);
 long ipc_put2lcl(int first,
							proxy_hdr_t __user * usr_hdr_ptr, 
							proxy_payload_t  __user * usr_pay_ptr, 
							void __user * ptr,
							long fifth);
 long ipc_get2rmt(int first,	
							proxy_hdr_t __user * usr_hdr_ptr, 
							proxy_payload_t  __user * usr_pay_ptr, 
							void __user * ptr,
							long timeout_ms);
 long ipc_add_node(	int dcid, 
								int nodeid,
								unsigned long third, 
								void __user * ptr, 
								long fifth);
 long ipc_del_node(	int dcid, 
								int nodeid,
								unsigned long third, 
								void __user * ptr, 
								long fifth);
 long ipc_dvs_init(int nodeid, 
							unsigned long second,
							unsigned long third,  
							dvs_usr_t __user *du_addr,
							long fifth);
 long ipc_dvs_end(	int first, 
								unsigned long second,
								unsigned long third, 
								void __user * ptr, 
								long fifth);
 long ipc_getep(	int pid, 
							unsigned long second,
							unsigned long third, 
							void __user * ptr, 
							long fifth);
 long ipc_getdvsinfo(	int first, 
								unsigned long second,
								unsigned long third,
								dvs_usr_t __user *dvs_usr_ptr, 
								long fifth);
 long ipc_proxy_conn(	int px_nr, 
								int status,
								unsigned long third, 
								void __user * ptr, 
								long fifth);
 long ipc_wait4bind(int oper, 
							int other_ep, 
							long timeout_ms,
							void __user * ptr, 
							long fifth);
 long ipc_migrate(	int oper, 
								int pid, 
								int dcid, 
								int endpoint, 
								int new_nodeid);
 long ipc_node_up(	int nodeid, 
								int px_nr, 
								unsigned long third, 
								char __user *node_name, 
								long fifth);
 long ipc_node_down(	int nodeid,
								unsigned long second,
								unsigned long third, 
								void __user * ptr,
								long fifth);
 long ipc_getproxyinfo(int px_nr,  
								struct proc_usr __user *sproc_usr_ptr, 
								struct proc_usr __user *rproc_usr_ptr,
								void __user * ptr, 
								long fifth);
 long ipc_wakeup(	int dcid, 
							int proc_ep,
							unsigned long third, 
							void __user * ptr, 
							long fifth);
 long new_exit_unbind(long code);

								
	
	