#include "dvk_mod.h"

// asmlinkage long new_dc_init(dc_usr_t *dcu_addr){return EDVSNOSYS;}
//asmlinkage long new_mini_send(int dst_ep, message* m_ptr, long timeout_ms){return EDVSNOSYS;}
//asmlinkage long new_mini_receive(int src_ep, message* m_ptr, long timeout_ms){return EDVSNOSYS;}
asmlinkage long new_void3(void){return EDVSNOSYS;}
//asmlinkage long new_mini_notify(int src_ep, int dst_ep, int value){return EDVSNOSYS;}
//asmlinkage long new_mini_sendrec(int srcdst_ep, message* m_ptr, long timeout_ms){return EDVSNOSYS;}
//asmlinkage long new_mini_rcvrqst(message* m_ptr, long timeout_ms){return EDVSNOSYS;}
//asmlinkage long new_mini_reply(int dst_ep, message* m_ptr, long timeout_ms){return EDVSNOSYS;}
//asmlinkage long new_dc_end(int dcid){return EDVSNOSYS;}
// asmlinkage long new_bind(int oper, int dcid, int pid, int proc, int nodeid){return EDVSNOSYS;}
// asmlinkage long new_unbind(int dcid, int proc_ep, long timeout_ms ){return EDVSNOSYS;}
asmlinkage long new_dc_dump(int dcid){return EDVSNOSYS;}
asmlinkage long new_proc_dump(int dcid){return EDVSNOSYS;}
//asmlinkage long new_getpriv(int dcid, int proc_ep, priv_usr_t *u_priv){return EDVSNOSYS;}
//asmlinkage long new_setpriv(int dcid, int proc_ep, priv_usr_t *u_priv){return EDVSNOSYS;}
//asmlinkage long new_vcopy(int src_ep, char *src_addr, int dst_ep,char *dst_addr, int bytes){return EDVSNOSYS;}
//asmlinkage long new_getdcinfo(int dcid, dc_usr_t *dc_usr_ptr){return EDVSNOSYS;}
//asmlinkage long new_getprocinfo(int dcid, int p_nr, struct proc_usr *proc_usr_ptr){return EDVSNOSYS;}
asmlinkage long new_void18(void){return EDVSNOSYS;}
//asmlinkage long new_mini_relay(int dst_ep, message* m_ptr){return EDVSNOSYS;}
//asmlinkage long new_proxies_bind(char *px_name, int px_nr, int spid, int rpid, int maxbuf){return EDVSNOSYS;}
//asmlinkage long new_proxies_unbind(int px_nr){return EDVSNOSYS;}
//asmlinkage long new_getnodeinfo(int nodeid, node_usr_t *node_usr_ptr){return EDVSNOSYS;}
//asmlinkage long new_put2lcl(proxy_hdr_t *usr_hdr_ptr, proxy_payload_t *usr_pay_ptr){return EDVSNOSYS;}
//asmlinkage long new_get2rmt(proxy_hdr_t *usr_hdr_ptr, proxy_payload_t *usr_pay_ptr, long timeout_ms){return EDVSNOSYS;}
//asmlinkage long new_add_node(int dcid, int nodeid){return EDVSNOSYS;}
//asmlinkage long new_del_node(int dcid, int nodeid){return EDVSNOSYS;}
//asmlinkage long new_dvs_init(int nodeid, dvs_usr_t *du_addr){return EDVSNOSYS;}
//asmlinkage long new_dvs_end(void){return EDVSNOSYS;}
//asmlinkage long new_getep(int pid){return EDVSNOSYS;}
//asmlinkage long new_getdvsinfo(dvs_usr_t *dvs_usr_ptr){return EDVSNOSYS;}
//asmlinkage long new_proxy_conn(int px_nr, int status){return EDVSNOSYS;}
// asmlinkage long new_wait4bind(int oper, int endpoint, long timeout_ms){return EDVSNOSYS;}
// asmlinkage long new_migrate(int oper, int pid, int dcid, int endpoint, int new_nodeid){return EDVSNOSYS;}   
//asmlinkage long new_node_up(char *node_name, int nodeid, int px_nr){return EDVSNOSYS;}
//asmlinkage long new_node_down(int nodeid){return EDVSNOSYS;}
//asmlinkage long new_getproxyinfo(int px_nr,  struct proc_usr *sproc_usr_ptr, struct proc_usr *rproc_usr_ptr){return EDVSNOSYS;}
//asmlinkage long new_wakeup(int dcid, int proc_ep){return EDVSNOSYS;}
//asmlinkage long new_exit_unbind(long code){return EDVSNOSYS;}
	
	