/* stub_dvkcall.c */

#ifndef _STUB_DVKCALL_H
#define _STUB_DVKCALL_H

#define DVS_USERSPACE	1

#define _GNU_SOURCE          /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>    /* For SYS_xxx definitions */

#include "../include/com/dvs_config.h"
#include "../include/com/config.h"
#include "../include/com/const.h"
#include "../include/com/com.h"
#include "../include/com/cmd.h"
#include "../include/com/proc_sts.h"
#include "../include/com/proc_usr.h"
#include "../include/com/proxy_sts.h"
#include "../include/com/proxy_usr.h"
#include "../include/com/dc_usr.h"
#include "../include/com/node_usr.h"
#include "../include/com/priv_usr.h"
#include "../include/com/dvs_usr.h"
#include "../include/com/dvk_calls.h"
#include "../include/com/dvk_ioctl.h"
#include "../include/com/dvs_errno.h"
#include "../include/com/ipc.h"
#include "../include/dvk/dvk_ioparm.h"
#include "../include/generic/tracker.h"

#include "stub_debug.h"

int dvk_fd;
int local_nodeid;

long dvk_sendrec_T(int endpoint , message *mptr, long timeout);


long dvk_open(void)
{
	LIBDEBUG(DBGPARAMS,  "Open dvk device file %s\n", DVK_FILE_NAME);
	dvk_fd = open(DVK_FILE_NAME, 0);
	if (dvk_fd < 0)  ERROR_RETURN(-errno);
	errno = 0;
	return(OK);
}

long dvk_vcopy(int src_ep, void *src_addr, int dst_ep, void *dst_addr, int bytes)
{
    long ret;
	vcopy_t parm;
    LIBDEBUG(DBGPARAMS, "src_ep=%d dst_ep=%d bytes=%d\n", 
		 src_ep, dst_ep, bytes);
	parm.v_src	= src_ep;	
	parm.v_dst	= dst_ep;	
	parm.v_rqtr	= SELF;	
	parm.v_saddr= src_addr;	
	parm.v_daddr= dst_addr;	
	parm.v_bytes= bytes;	
	ret = ioctl(dvk_fd,DVK_IOCSVCOPY, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_dvs_end(void)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "\n");
	ret = ioctl(dvk_fd,DVK_IOCTDVSEND);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_dc_init(dc_usr_t *dcu_ptr)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "\n");
	ret = ioctl(dvk_fd,DVK_IOCSDCINIT, (int) dcu_ptr);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_dc_end(int dcid)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "dcid=%d\n", dcid);
	ret = ioctl(dvk_fd,DVK_IOCTDCEND, dcid);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_getep(int pid)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "pid=%d\n", pid);
	ret = ioctl(dvk_fd,DVK_IOCQGETEP, pid);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_getdvsinfo(dvs_usr_t *dvsu_ptr)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "\n");
	ret = ioctl(dvk_fd,DVK_IOCGGETDVSINFO, (int) dvsu_ptr);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d\n",ret); 
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_proxies_unbind(int pxid)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "pxid=%d\n", pxid);
	ret = ioctl(dvk_fd,DVK_IOCTPROXYUNBIND, pxid);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_node_down(int nodeid)
{
    long ret;
    LIBDEBUG(DBGPARAMS, "nodeid=%d\n", nodeid);
	ret = ioctl(dvk_fd,DVK_IOCTNODEDOWN, nodeid);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

#define dvk_rcvrqst(m) 		dvk_rcvrqst_T( m, TIMEOUT_FOREVER)
long dvk_rcvrqst_T(message *mptr, long timeout)
{
    long ret;
	parm_rcvrqst_t parm;
	
    LIBDEBUG(DBGPARAMS, "timeout=%ld\n", timeout);
	parm.parm_mptr	= mptr;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSRCVRQST, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_getdcinfo(int dcid, dc_usr_t *dcu_ptr)
{
    long ret;
	parm_getdcinfo_t parm;
    LIBDEBUG(DBGPARAMS, "dcid=%d\n", dcid);
	parm.parm_dcid	= dcid;
	parm.parm_dc	= dcu_ptr;
	ret = ioctl(dvk_fd,DVK_IOCGGETDCINFO, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_getnodeinfo(int nodeid, node_usr_t *node_ptr)
{
    long ret;
	parm_getnodeinfo_t parm;
    LIBDEBUG(DBGPARAMS, "nodeid=%d\n", nodeid);
	parm.parm_nodeid	= nodeid;
	parm.parm_node		= node_ptr;
	ret = ioctl(dvk_fd,DVK_IOCGGETNODEINFO, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_relay(int endpoint, message *mptr)
{
    long ret;
	parm_relay_t parm;
    LIBDEBUG(DBGPARAMS, "endpoint=%d\n", endpoint);
	parm.parm_ep	= endpoint;
	parm.parm_mptr	= mptr;
	ret = ioctl(dvk_fd,DVK_IOCSRELAY, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_wakeup(int dcid, int dst_ep)
{
    long ret;
	parm_wakeup_t parm;
    LIBDEBUG(DBGPARAMS, "dcid=%d dst_ep=%d\n", dcid, dst_ep);
	parm.parm_dcid  = dcid;
	parm.parm_ep	= dst_ep;
	ret = ioctl(dvk_fd,DVK_IOCSWAKEUP, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_put2lcl(cmd_t *header, proxy_payload_t *payload)
{
    long ret;
	parm_put2lcl_t parm;
    LIBDEBUG(DBGPARAMS, "\n");
	parm.parm_cmd  = header;
	parm.parm_pay  = payload;
	ret = ioctl(dvk_fd,DVK_IOCSPUT2LCL, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_add_node(int dcid, int nodeid)
{
    long ret;
	parm_dcnode_t parm;
    LIBDEBUG(DBGPARAMS, "dcid=%d nodeid=%d\n", dcid, nodeid);
	parm.parm_dcid  	= dcid;
	parm.parm_nodeid  	= nodeid;
	ret = ioctl(dvk_fd,DVK_IOCSADDNODE, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_del_node(int dcid, int nodeid)
{
    long ret;
	parm_dcnode_t parm;
    LIBDEBUG(DBGPARAMS, "dcid=%d nodeid=%d\n", dcid, nodeid);
	parm.parm_dcid  	= dcid;
	parm.parm_nodeid  	= nodeid;
	ret = ioctl(dvk_fd,DVK_IOCSDELNODE, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_dvs_init(int nodeid, dvs_usr_t *dvsu_ptr)
{
    long ret;
	parm_dvsinit_t parm;
    LIBDEBUG(DBGPARAMS, "nodeid=%d\n", nodeid);
	parm.parm_nodeid	= nodeid;
	parm.parm_dvs		= dvsu_ptr;
	ret = ioctl(dvk_fd,DVK_IOCSDVSINIT, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_proxy_conn(int pxid, int status)
{
    long ret;
	parm_pxconn_t parm;
    LIBDEBUG(DBGPARAMS, "pxid=%d\n", pxid);
	parm.parm_pxid  = pxid;
	parm.parm_sts  	= status;
	ret = ioctl(dvk_fd,DVK_IOCSPROXYCONN, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

#define dvk_wait4bind()						dvk_wait4bindep_X(WAIT_BIND,  SELF,  TIMEOUT_FOREVER)
#define dvk_wait4bindep(endpoint)			dvk_wait4bindep_X(WAIT_BIND,  endpoint,  TIMEOUT_FOREVER)
#define dvk_wait4unbind(endpoint)			dvk_wait4bindep_X(WAIT_UNBIND, endpoint, TIMEOUT_FOREVER)
#define dvk_wait4bind_T(to_ms)				dvk_wait4bindep_X(WAIT_BIND, SELF, to_ms)
#define dvk_wait4bindep_T(endpoint, to_ms)	dvk_wait4bindep_X(WAIT_BIND,  endpoint, to_ms)
#define dvk_wait4unbind_T(endpoint, to_ms)	dvk_wait4bindep_X(WAIT_UNBIND, endpoint, to_ms)
long dvk_wait4bindep_X(int cmd, int endpoint, unsigned long timeout)
{
    long ret, rcode, caller_ep, old_errno;
	parm_wait4bind_t parm;
	dc_usr_t dcu, *dcu_ptr;
	proc_usr_t proc, *proc_ptr;
	message msg, *msg_ptr;
	
    LIBDEBUG(DBGPARAMS, "cmd=%d endpoint=%d timeout=%ld\n", cmd, endpoint, timeout);
	parm.parm_cmd  	= cmd;
	parm.parm_ep  	= endpoint;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSWAIT4BIND, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	
	if( cmd == WAIT_BIND){
		if( ret < 0){
			if( errno == (-endpoint) ){
				errno = 0;
				return(endpoint);
			}else{
				old_errno = errno;

				caller_ep = dvk_getep((pid_t) syscall (SYS_gettid));
				if( (endpoint == SELF) 
				||  (endpoint == caller_ep)) 
					ERROR_RETURN(-old_errno);	
				
				// get DC INFO 
				dcu_ptr = &dcu;
				rcode = dvk_getdcinfo(PROC_NO_PID, dcu_ptr);
				if( rcode < 0) 
					ERROR_RETURN(-old_errno);	
								
				// Request TRACKER for the NODEID of an endpoint 	
				msg_ptr                 = &msg;
				msg_ptr->m_type   	    = TRACKER_REQUEST;
				msg_ptr->rqtr_ep  	    = caller_ep;
				msg_ptr->rqtr_nodeid    = local_nodeid;
				msg_ptr->sch_ep   	    = endpoint;
				msg_ptr->rply_nodeid    = (-1);
				msg_ptr->sch_rts_flags  = 0;
				msg_ptr->sch_misc_flags = 0;
				LIBDEBUG(DBGPARAMS,TRK_FORMAT,TRK_FIELDS(msg_ptr));		
				rcode = dvk_sendrec_T(TRACKER_EP(local_nodeid), msg_ptr, TIMEOUT_MOLCALL);
				if( rcode < EDVSERRCODE || rcode == EDVSTIMEDOUT )
					ERROR_RETURN(-old_errno);
					
				// TRACKER reply with an error. It must reply with NODEID
				if( msg_ptr->m_type !=  TRACKER_REPLY )
					ERROR_RETURN(-old_errno);

				// nodeid must be different  PROC_NO_PID (-1)
				if( msg_ptr->rply_nodeid == PROC_NO_PID)
					ERROR_RETURN(-old_errno);
				
				// nodeid must be different from local_nodeid
				if( msg_ptr->rply_nodeid == local_nodeid)
					ERROR_RETURN(-old_errno);
				
				// nodeid must be a valid node number in the DC 
				if( msg_ptr->rply_nodeid < 0 
				||  msg_ptr->rply_nodeid >= dcu_ptr->dc_nr_nodes)
					ERROR_RETURN(-old_errno);
				
				// the nodeid must be one of the valid nodes of the DC 
				if( ! TEST_BIT(dcu_ptr->dc_nodes, msg_ptr->rply_nodeid))
					ERROR_RETURN(-old_errno);			
							
				// retry the WAIT4BIND 
				parm.parm_cmd  	= WAIT_BIND;
				parm.parm_ep  	= endpoint;
				parm.parm_tout	= TIMEOUT_NOWAIT;
				ret = ioctl(dvk_fd,DVK_IOCSWAIT4BIND, (int) &parm);
				LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);
				if ( ret < 0) {
					if( errno == (-endpoint) ){
						errno = 0;
						return(endpoint);
					}
				}
				errno = 0;
				return(ret); 
			}
		}else{ 
			errno = 0;
			return(ret);
		}
	} else{ // WAIT_UNBIND
		if (ret < 0) {
			ERROR_RETURN(-errno);
		}else{
			errno = 0;
			return(ret);
		}
	}
}

#define dvk_unbind(dcid,p_ep) 		  		stub_dvkcall3(DVK_UNBIND, dcid, p_ep, TIMEOUT_FOREVER)
long dvk_unbind_T(int dcid, int endpoint, unsigned long timeout)
{
    long ret;
	parm_unbind_t parm;
    LIBDEBUG(DBGPARAMS, "dcid=%d endpoint=%d timeout=%ld\n", dcid, endpoint, timeout);
	parm.parm_dcid  = dcid;
	parm.parm_ep  	= endpoint;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSUNBIND, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

#define dvk_send(dst_ep,m)			dvk_send_T(dst_ep, (int) m, TIMEOUT_FOREVER)
long dvk_send_T(int endpoint , message *mptr, long timeout)
{
    long ret;
	parm_ipc_t parm;
    LIBDEBUG(DBGPARAMS, "endpoint=%d timeout=%ld\n", endpoint, timeout);
	parm.parm_ep	= endpoint;
	parm.parm_mptr	= mptr;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSSEND, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

#define dvk_receive(src_ep,m)   	dvk_receive_T(src_ep, (int) m, TIMEOUT_FOREVER)
long dvk_receive_T(int endpoint , message *mptr, long timeout)
{
    long ret;
	parm_ipc_t parm;
	
    LIBDEBUG(DBGPARAMS, "endpoint=%d timeout=%ld\n", endpoint, timeout);
	parm.parm_ep	= endpoint;
	parm.parm_mptr	= mptr;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSRECEIVE, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

#define dvk_sendrec(srcdst_ep,m) 	dvk_sendrec_T(srcdst_ep, (int) m, TIMEOUT_FOREVER)
long dvk_sendrec_T(int endpoint , message *mptr, long timeout)
{
    long ret;
	parm_ipc_t parm;
	
    LIBDEBUG(DBGPARAMS, "endpoint=%d timeout=%ld\n", endpoint, timeout);
	parm.parm_ep	= endpoint;
	parm.parm_mptr	= mptr;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSSENDREC, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

#define dvk_reply(dst_ep,m)			dvk_reply_T(dst_ep, (int) m, TIMEOUT_FOREVER)
long dvk_reply_T(int endpoint , message *mptr, long timeout)
{
    long ret;
	parm_ipc_t parm;
	
    LIBDEBUG(DBGPARAMS, "endpoint=%d timeout=%ld\n", endpoint, timeout);
	parm.parm_ep	= endpoint;
	parm.parm_mptr	= mptr;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCSREPLY, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

#define dvk_notify(dst_ep)						dvk_notify_X(SELF, dst_ep, HARDWARE)
#define dvk_hdw_notify(dcid, dst_ep) 			dvk_notify_X(HARDWARE, dst_ep, dcid)
#define dvk_ntfy_value(src_nr, dst_ep, value)	dvk_notify_X(src_nr, dst_ep, value)
#define dvk_src_notify(src_nr, dst_ep)			dvk_notify_X(src_nr, dst_ep, HARDWARE)
long dvk_notify_X(int nr , int endpoint, int value)
{
    long ret;
	parm_notify_t parm;
	
    LIBDEBUG(DBGPARAMS, "nr=%d endpoint=%d value=%d\n", nr, endpoint, value);
	parm.parm_nr	= nr;
	parm.parm_ep	= endpoint;
	parm.parm_val	= value;
	ret = ioctl(dvk_fd,DVK_IOCSNOTIFY, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

long dvk_setpriv(int dcid , int endpoint, priv_usr_t *priv)
{
    long ret;
	parm_priv_t parm;
    LIBDEBUG(DBGPARAMS, "dcid=%d endpoint=%d \n", dcid, endpoint);
	parm.parm_dcid  = dcid;
	parm.parm_ep	= endpoint;
	parm.parm_priv	= priv;
	ret = ioctl(dvk_fd,DVK_IOCSSETPRIV, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

long dvk_getpriv(int dcid , int endpoint, priv_usr_t *priv)
{
    long ret;
	parm_priv_t parm;
	
    LIBDEBUG(DBGPARAMS, "dcid=%d endpoint=%d \n", dcid, endpoint);
	parm.parm_dcid  = dcid;
	parm.parm_ep	= endpoint;
	parm.parm_priv	= priv;
	ret = ioctl(dvk_fd,DVK_IOCGGETPRIV, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(OK);
}

#define dvk_get2rmt(header, payload)   	stub_dvkcall3(GET2RMT, (int)header, (int)payload, HELLO_PERIOD)
long dvk_get2rmt_T(cmd_t *header, proxy_payload_t *payload , long timeout)
{
    long ret;
	parm_get2rmt_t parm;
	
    LIBDEBUG(DBGPARAMS, "timeout=%ld\n",timeout);
	parm.parm_cmd  	= header;
	parm.parm_pay  	= payload;
	parm.parm_tout	= timeout;
	ret = ioctl(dvk_fd,DVK_IOCGGET2RMT, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_node_up(char *name, int nodeid,  int pxid)
{
    long ret;
	parm_nodeup_t parm;
	
    LIBDEBUG(DBGPARAMS, "nodeid=%d pxid=%d \n", nodeid, pxid);
	parm.parm_name 	= name;
	parm.parm_nodeid= nodeid;
	parm.parm_pxid	= pxid;
	ret = ioctl(dvk_fd,DVK_IOCSNODEUP, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_getproxyinfo(int pxid, proc_usr_t *sproc_usr, proc_usr_t *rproc_usr)
{
    long ret;
	parm_proxyinfo_t parm;

    LIBDEBUG(DBGPARAMS, "pxid=%d \n", pxid);
	parm.parm_pxid	= pxid;
	parm.parm_spx	= sproc_usr;
	parm.parm_rpx	= rproc_usr;
	ret = ioctl(dvk_fd,DVK_IOCGGETPXINFO, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

long dvk_getprocinfo(int dcid, int p_nr, proc_usr_t *p_usr)
{
    long ret;
	parm_procinfo_t parm;

    LIBDEBUG(DBGPARAMS, "dcid=%d p_nr=%d \n", dcid, p_nr);
	parm.parm_dcid	= dcid;
	parm.parm_nr	= p_nr;
	parm.parm_proc	= p_usr;
	ret = ioctl(dvk_fd,DVK_IOCGGETPRINFO, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

#define dvk_bind(dcid,endpoint) 			dvk_bind_X(SELF_BIND, dcid, getpid(), endpoint, LOCALNODE)
#define dvk_tbind(dcid,endpoint) 			dvk_bind_X(SELF_BIND, dcid, (pid_t) syscall (SYS_gettid), endpoint, LOCALNODE)
#define dvk_lclbind(dcid,pid,endpoint) 		dvk_bind_X(LCL_BIND, dcid, pid, endpoint, LOCALNODE)
#define dvk_rmtbind(dcid,name,endpoint,nodeid) 	dvk_bind_X(RMT_BIND, dcid, (int) name, endpoint, nodeid)
#define dvk_bkupbind(dcid,pid,endpoint,nodeid) 	dvk_bind_X(BKUP_BIND, dcid, pid, endpoint, nodeid)
#define dvk_replbind(dcid,pid,endpoint) 	dvk_bind_X(REPLICA_BIND, dcid, pid, endpoint, LOCALNODE)
long dvk_bind_X(int cmd, int dcid, int pid, int endpoint, int nodeid)
{
    long ret;
	parm_bind_t parm;
	
    LIBDEBUG(DBGPARAMS, "cmd=%d dcid=%d pid=%d endpoint=%d nodeid=%d\n", 
		cmd, dcid, pid, endpoint, nodeid);
	
	parm.parm_cmd	= cmd;	
	parm.parm_dcid	= dcid;	
	parm.parm_pid	= pid;	
	parm.parm_ep	= endpoint;	
	parm.parm_nodeid= nodeid;	
	errno = 0;
	ret = ioctl(dvk_fd,DVK_IOCSDVKBIND, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
 
	if( ret == (-1)){
		if( -errno == endpoint){
			errno = 0;
			return(endpoint);
		} 
		if (  -errno < EDVSERRCODE) ERROR_RETURN(-errno); 
	}
	errno = 0;
	return(ret);
}

long dvk_proxies_bind(char *name, int pxid, int spid, int rpid, int maxcopybuf)
{
    long ret;
	parm_pxbind_t parm;

    LIBDEBUG(DBGPARAMS, "name=%s pxid=%d spid=%d rpid=%d maxcopybuf=%d\n", 
		 name, pxid, spid, rpid, maxcopybuf);
		 
	parm.parm_name  = name;	
	parm.parm_pxid	= pxid;	
	parm.parm_spid	= spid;	
	parm.parm_rpid	= rpid;	
	parm.parm_maxbuf= maxcopybuf;	
	ret = ioctl(dvk_fd,DVK_IOCSPROXYBIND, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}

#define dvk_migr_start(dcid, ep)	dvk_migrate_X(MIGR_START, PROC_NO_PID, dcid, ep, PROC_NO_PID)
#define dvk_migr_rollback(dcid, ep) dvk_migrate_X(MIGR_ROLLBACK, PROC_NO_PID, dcid, ep, PROC_NO_PID)
#define dvk_migr_commit(pid, dcid, ep, new_node)	dvk_migrate_X(MIGR_COMMIT, pid, dcid, ep, new_node)
long dvk_migrate_X(int cmd, int pid, int dcid, int endpoint, int nodeid)
{
    long ret;
	parm_bind_t parm;

    LIBDEBUG(DBGPARAMS, "cmd=%d pid=%d dcid=%d endpoint=%d nodeid=%d\n", 
		 cmd, pid, dcid, endpoint, nodeid);
	parm.parm_cmd	= cmd;	
	parm.parm_pid	= pid;	
	parm.parm_dcid	= dcid;	
	parm.parm_ep	= endpoint;	
	parm.parm_nodeid= nodeid;	
	ret = ioctl(dvk_fd,DVK_IOCSMIGRATE, (int) &parm);
    LIBDEBUG(DBGPARAMS,"ioctl ret=%d errno=%d\n",ret, errno);	
	if (ret < 0) ERROR_RETURN(-errno); 
	errno = 0;
	return(ret);
}
				


#endif /* _STUB_DVKCALL_H */
