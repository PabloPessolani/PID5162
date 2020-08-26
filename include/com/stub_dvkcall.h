/* stub_dvkcall.h */

#ifndef _DVKCALL_H_
#define _DVKCALL_H_

long dvk_open(void);

long dvk_dc_dump(void);
long dvk_dvs_end(void);
long dvk_dc_init(dc_usr_t *dcu_ptr);
long dvk_dc_end(int dcid);
long dvk_proc_dump(int dcid);
long dvk_getep(int pid);
long dvk_getdvsinfo(dvs_usr_t *dvsu_ptr);
long dvk_proxies_unbind(int pxid);
long dvk_node_down(int nodeid);
#define dvk_rcvrqst(m) 		dvk_rcvrqst_T( m, TIMEOUT_FOREVER)
long dvk_getdcinfo(int dcid, dc_usr_t *dcu_ptr);
long dvk_getnodeinfo(int nodeid, node_usr_t *nodeu_ptr);
long dvk_relay(int endpoint, message *mptr);
long dvk_wakeup(int dcid, int dst_ep);
long dvk_put2lcl(cmd_t *header,  proxy_payload_t *payload);
long dvk_delnode(int dcid, int nodeid);
long dvk_dvs_init(int nodeid, dvs_usr_t *dvsu_ptr);
long dvk_proxy_conn(int pxid, int status);
#define dvk_wait4bind()						dvk_wait4bindep_X(WAIT_BIND,  SELF,  TIMEOUT_FOREVER)
#define dvk_wait4bindep(endpoint)			dvk_wait4bindep_X(WAIT_BIND,  endpoint,  TIMEOUT_FOREVER)
#define dvk_wait4unbind(endpoint)			dvk_wait4bindep_X(WAIT_UNBIND, endpoint, TIMEOUT_FOREVER)
#define dvk_wait4bind_T(to_ms)				dvk_wait4bindep_X(WAIT_BIND, SELF, to_ms)
#define dvk_wait4bindep_T(endpoint, to_ms)	dvk_wait4bindep_X(WAIT_BIND,  endpoint, to_ms)
#define dvk_wait4unbind_T(endpoint, to_ms)	dvk_wait4bindep_X(WAIT_UNBIND, endpoint, to_ms)
long dvk_wait4bindep_X(int cmd, int endpoint, unsigned long timeout);
#define dvk_unbind(dcid,p_ep) 		  		dvk_unbind_T(dcid, p_ep, TIMEOUT_FOREVER)
long dvk_unbind_T(int dcid, int endpoint, unsigned long timeout);
#define dvk_send(dst_ep,m)			dvk_send_T(dst_ep, (int) m, TIMEOUT_FOREVER)
long dvk_send_T(int endpoint , message *mptr, long timeout);
#define dvk_receive(src_ep,m)   	dvk_receive_T(src_ep, (int) m, TIMEOUT_FOREVER)
long dvk_receive_T(int endpoint , message *mptr, long timeout);
#define dvk_sendrec(srcdst_ep,m) 	dvk_sendrec_T(srcdst_ep, (int) m, TIMEOUT_FOREVER)
long dvk_sendrec_T(int endpoint , message *mptr, long timeout);
#define dvk_reply(dst_ep,m)			dvk_reply_T(dst_ep, (int) m, TIMEOUT_FOREVER)
long dvk_reply_T(int endpoint , message *mptr, long timeout);
#define dvk_notify(dst_ep)						dvk_notify_X(SELF, dst_ep, HARDWARE)
#define dvk_hdw_notify(dcid, dst_ep) 			dvk_notify_X(HARDWARE, dst_ep, dcid)
#define dvk_ntfy_value(src_nr, dst_ep, value)	dvk_notify_X(src_nr, dst_ep, value)
#define dvk_src_notify(src_nr, dst_ep)			dvk_notify_X(src_nr, dst_ep, HARDWARE)
long dvk_notify_X(int nr , int endpoint, int value);
long dvk_setpriv(int dcid , int endpoint, priv_usr_t *priv);
long dvk_getpriv(int dcid , int endpoint, priv_usr_t *priv);
#define dvk_get2rmt(header, payload)   		dvk_get2rmt_T(header, payload, HELLO_PERIOD)
long dvk_get2rmt_T(cmd_t *header, proxy_payload_t *payload , long timeout);
long dvk_node_up(char *name, int nodeid,  int pxid);
long dvk_getproxyinfo(int pxid, proc_usr_t *sproc_usr, proc_usr_t *rproc_usr);
#define dvk_bind(dcid,endpoint) 			dvk_bind_X(SELF_BIND, dcid, (-1), endpoint, LOCALNODE)

#ifndef  CONFIG_UML_DVK
#define dvk_tbind(dcid,endpoint) 			dvk_bind_X(SELF_BIND, dcid, (pid_t) syscall (SYS_gettid), endpoint, LOCALNODE)
#else // CONFIG_UML_DVK
#define dvk_tbind(dcid,endpoint) 			dvk_bind_X(SELF_BIND, dcid, (pid_t) os_gettid(), endpoint, LOCALNODE)
#endif // CONFIG_UML_DVK

#define dvk_lclbind(dcid,pid,endpoint) 		dvk_bind_X(LCL_BIND, dcid, pid, endpoint, LOCALNODE)
#define dvk_rmtbind(dcid,name,endpoint,nodeid) 	dvk_bind_X(RMT_BIND, dcid, (int) name, endpoint, nodeid)
#define dvk_bkupbind(dcid,pid,endpoint,nodeid) 	dvk_bind_X(BKUP_BIND, dcid, pid, endpoint, nodeid)
#define dvk_replbind(dcid,pid,endpoint) 	dvk_bind_X(REPLICA_BIND, dcid, pid, endpoint, LOCALNODE)
#define dvk_ukbind(dcid,pid,endpoint) 	    dvk_bind_X(UNIKERNEL_BIND, dcid, pid, endpoint, LOCALNODE)
long dvk_bind_X(int cmd, int dcid, int pid, int endpoint, int nodeid);
long dvk_proxies_bind(char *name, int pxid, int spid, int rpid, int maxcopybuf);
#define dvk_migr_start(dcid, ep)	dvk_migrate_X(MIGR_START, PROC_NO_PID, dcid, ep, PROC_NO_PID)
#define dvk_migr_rollback(dcid, ep) dvk_migrate_X(MIGR_ROLLBACK, PROC_NO_PID, dcid, ep, PROC_NO_PID)
#define dvk_migr_commit(pid, dcid, ep, new_node)	dvk_migrate_X(MIGR_COMMIT, pid, dcid, ep, new_node)
long dvk_migrate_X(int cmd, int pid, int dcid, int endpoint, int nodeid);
long dvk_vcopy(int src_ep, void *src_addr, int dst_ep, void *dst_addr, int bytes);
long dvk_getprocinfo(int dcid, int p_nr, proc_usr_t *p_usr);

#endif // _DVKCALL_H_


































