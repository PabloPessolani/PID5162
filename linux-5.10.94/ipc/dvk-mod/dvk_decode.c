#include "dvk_mod.h"


long io_void0(unsigned long arg)
{  
	DVKDEBUG(DBGPARAMS,"\n");
	return(EDVSNOSYS);
}

long io_dc_init(unsigned long arg)
{  
	dc_usr_t *dcu_ptr; 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	dcu_ptr = (dc_usr_t *) arg;
	rcode = new_dc_init(dcu_ptr);
	return(rcode);
}

long io_mini_send(unsigned long arg)
{  
	parm_ipc_t ipc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &ipc, (const void __user *) arg, sizeof(parm_ipc_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_mini_send(ipc.parm_ep, ipc.parm_mptr, ipc.parm_tout);
	return(rcode);
}

long io_mini_receive(unsigned long arg)
{  
	parm_ipc_t ipc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &ipc, (const void __user *) arg, sizeof(parm_ipc_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);		
	rcode = new_mini_receive(ipc.parm_ep, ipc.parm_mptr, ipc.parm_tout);
	return(rcode);
}

long io_mini_notify(unsigned long arg){
	parm_notify_t nfy;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &nfy, (const void __user *) arg, sizeof(parm_notify_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_mini_notify(nfy.parm_nr, nfy.parm_ep, nfy.parm_val);
	return(rcode);
}

long io_mini_sendrec(unsigned long arg)
{  
	parm_ipc_t ipc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &ipc, (const void __user *) arg, sizeof(parm_ipc_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);		
	rcode = new_mini_sendrec(ipc.parm_ep, ipc.parm_mptr, ipc.parm_tout);
	return(rcode);
}

long io_mini_rcvrqst(unsigned long arg)
{  
	parm_rcvrqst_t rr;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &rr, (const void __user *) arg, sizeof(parm_rcvrqst_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_mini_rcvrqst(rr.parm_mptr, rr.parm_tout);
	return(rcode);
}

long io_mini_reply(unsigned long arg)
{  
	parm_ipc_t ipc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &ipc, (const void __user *) arg, sizeof(parm_ipc_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);		
	rcode = new_mini_reply(ipc.parm_ep, ipc.parm_mptr, ipc.parm_tout);
	return(rcode);
}

long io_dc_end(unsigned long arg)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_dc_end((int) arg);
	return(rcode);
}

long io_bind(unsigned long arg)
{  
	parm_bind_t bd;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &bd, (const void __user *) arg, sizeof(parm_bind_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_bind(bd.parm_cmd, bd.parm_dcid, bd.parm_pid,
						bd.parm_ep, bd.parm_nodeid);
	return(rcode);
}

long io_unbind(unsigned long arg)
{  
	parm_unbind_t ub;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &ub, (const void __user *) arg, sizeof(parm_unbind_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_unbind(ub.parm_dcid, ub.parm_ep, ub.parm_tout);
	return(rcode);
}

long io_getpriv(unsigned long arg)
{  
	parm_priv_t pv;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pv, (const void __user *) arg, sizeof(parm_priv_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_getpriv(pv.parm_dcid, pv.parm_ep, pv.parm_priv);
	return(rcode);
}

long io_setpriv(unsigned long arg)
{  
	parm_priv_t pv;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pv, (const void __user *) arg, sizeof(parm_priv_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);		
	rcode = new_setpriv(pv.parm_dcid, pv.parm_ep, pv.parm_priv);
	return(rcode);
}

long io_vcopy(unsigned long arg)
{  
	parm_vcopy_t vc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &vc, (const void __user *) arg, sizeof(parm_vcopy_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_vcopy(vc.v_src,  vc.v_saddr, vc.v_dst, vc.v_daddr, vc.v_bytes);
	return(rcode);
}

long io_getdcinfo(unsigned long arg)
{  
	parm_getdcinfo_t gdci;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &gdci, (const void __user *) arg, sizeof(parm_getdcinfo_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_getdcinfo(gdci.parm_dcid, gdci.parm_dc);
	return(rcode);
}

long io_getprocinfo(unsigned long arg)
{  
	parm_procinfo_t pi;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pi, (const void __user *) arg, sizeof(parm_procinfo_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_getprocinfo(pi.parm_dcid, pi.parm_nr, pi.parm_proc);
	return(rcode);
}

long io_mini_relay(unsigned long arg)
{  
	parm_relay_t rly;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &rly, (const void __user *) arg, sizeof(parm_relay_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_mini_relay(rly.parm_ep, rly.parm_mptr);
	return(rcode);
}

long io_proxies_bind(unsigned long arg)
{  
	parm_pxbind_t pb;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pb, (const void __user *) arg, sizeof(parm_pxbind_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_proxies_bind(pb.parm_name, pb.parm_pxid, pb.parm_spid,
						pb.parm_rpid, pb.parm_maxbuf);
	return(rcode);
}

long io_proxies_unbind(unsigned long arg)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_proxies_unbind((int) arg);
	return(rcode);
}

long io_getnodeinfo(unsigned long arg)
{  
	parm_getnodeinfo_t ni;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &ni, (const void __user *) arg, sizeof(parm_getnodeinfo_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_getnodeinfo(ni.parm_nodeid, ni.parm_node);
	return(rcode);
}

long io_put2lcl(unsigned long arg)
{  
	parm_put2lcl_t pl;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pl, (const void __user *) arg, sizeof(parm_put2lcl_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_put2lcl(pl.parm_cmd, pl.parm_pay);
	return(rcode);
}

long io_get2rmt(unsigned long arg)
{  
	parm_get2rmt_t gr;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &gr, (const void __user *) arg, sizeof(parm_get2rmt_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_get2rmt(gr.parm_cmd, gr.parm_pay, gr.parm_tout );
	return(rcode);
}

long io_add_node(unsigned long arg)
{
	parm_dcnode_t dcn;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &dcn, (const void __user *) arg, sizeof(parm_dcnode_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_add_node(dcn.parm_dcid, dcn.parm_nodeid);
	return(rcode);
}

long io_del_node(unsigned long arg)
{  
	parm_dcnode_t dcn;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &dcn, (const void __user *) arg, sizeof(parm_dcnode_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_del_node(dcn.parm_dcid, dcn.parm_nodeid);
	return(rcode);
}

long io_dvs_init(unsigned long arg)
{  
	parm_dvsinit_t dv;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &dv, (const void __user *) arg, sizeof(parm_dvsinit_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_dvs_init(dv.parm_nodeid, dv.parm_dvs);
	return(rcode);
}

long io_dvs_end(unsigned long arg)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_dvs_end();
	return(rcode);
}

long io_getep(unsigned long arg)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_getep((int) arg);
	return(rcode);
}
	
long io_getdvsinfo(unsigned long arg)
{  
	dvs_usr_t *dvs_ptr; 
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	dvs_ptr = (dvs_usr_t *) arg;
	rcode = new_getdvsinfo(dvs_ptr);
	return(rcode);
}

long io_proxy_conn(unsigned long arg)
{  
	parm_pxconn_t pc;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pc, (const void __user *) arg, sizeof(parm_pxconn_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_proxy_conn(pc.parm_pxid, pc.parm_sts);
	return(rcode);
}

long io_wait4bind(unsigned long arg)
{  
	parm_wait4bind_t w4;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &w4, (const void __user *) arg, sizeof(parm_wait4bind_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_wait4bind(w4.parm_cmd, w4.parm_ep, w4.parm_tout);
	return(rcode);
}

long io_migrate(unsigned long arg)
{  
	parm_bind_t mg;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &mg, (const void __user *) arg, sizeof(parm_bind_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_migrate(mg.parm_cmd, mg.parm_pid, mg.parm_dcid,
						mg.parm_ep, mg.parm_nodeid);
	return(rcode);
}

long io_node_up(unsigned long arg)
{  
	parm_nodeup_t nu;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &nu, (const void __user *) arg, sizeof(parm_nodeup_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);
	rcode = new_node_up(nu.parm_name, nu.parm_nodeid, nu.parm_pxid);
	return(rcode);
}

long io_node_down(unsigned long arg)
{  
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = new_node_down((int) arg);
	return(rcode);
}

long io_getproxyinfo(unsigned long arg)
{  
	parm_proxyinfo_t pi;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &pi, (const void __user *) arg, sizeof(parm_proxyinfo_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_getproxyinfo(pi.parm_pxid, pi.parm_spx, pi.parm_rpx);
	return(rcode);
}

long io_wakeup(unsigned long arg)
{  
	parm_wakeup_t wk;
	long rcode=0; 
	DVKDEBUG(DBGPARAMS,"\n");
	rcode = copy_from_user( (void*) &wk, (const void __user *) arg, sizeof(parm_wakeup_t));
	if(rcode) ERROR_RETURN(EDVS2BIG);	
	rcode = new_wakeup(wk.parm_dcid, wk.parm_ep);
	return(rcode);
}

long io_exit_unbind(unsigned long arg) 
{ 
long rcode=0; 
DVKDEBUG(DBGPARAMS,"\n");
return(rcode);
}

