/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Copyright (C) 2001 by various other people who didn't put their name here.
 * Licensed under the GPL.
 */

// #ifdef CONFIG_UML_NET_RTAP 

#define _RTAP_GLOBAL_HERE	1 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/ata.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <asm/tlbflush.h>
#include <kern_util.h>
#include <net_kern.h>
#include "mconsole_kern.h"
#include <init.h>
#include <irq_kern.h>
#include <os.h>
#include <net_user.h>

#include "rtap.h"

#ifdef  CONFIG_DVKIOCTL
#pragma message ("CONFIG_DVKIOCTL=YES")
#else // CONFIG_DVKIOCTL
#pragma message ("CONFIG_DVKIOCTL=NO")
#endif // CONFIG_DVKIOCTL

#ifdef  CONFIG_UML_DVK
#pragma message ("CONFIG_UML_DVK=YES")
#else // CONFIG_UML_DVK
#pragma message ("CONFIG_UML_DVK=NO")
#endif // CONFIG_UML_DVK

#ifdef  CONFIG_UML_USER
#pragma message ("CONFIG_UML_USER=YES")
#else // CONFIG_UML_USER
#pragma message ("CONFIG_UML_USER=NO")
#endif // CONFIG_UML_USER

#ifdef  CONFIG_DVKIPC
#pragma message ("CONFIG_DVKIPC=YES")
#else // CONFIG_DVKIPC
#pragma message ("CONFIG_DVKIPC=NO")
#endif // CONFIG_DVKIPC

#ifdef  CONFIG_UML_NET_RTAP
#pragma message ("CONFIG_UML_NET_RTAP=YES")
#else // CONFIG_UML_NET_RTAP
#pragma message ("CONFIG_UML_NET_RTAP=NO")
#endif // CONFIG_UML_NET_RTAP

static DEFINE_MUTEX(rt_main_mutex);
static DEFINE_MUTEX(rt_thread_mutex);

#define WAIT10SECS 	10000000000

int init_sync(void ){
	DVKDEBUG(INTERNAL,"\n");
	mutex_lock(&rt_main_mutex);
	return(OK);
}

int end_sync(void ){
	DVKDEBUG(INTERNAL,"\n");
	mutex_unlock(&rt_main_mutex);
	return(OK);
}

int enter_sync(void ){
	DVKDEBUG(INTERNAL,"\n");
	mutex_lock(&rt_thread_mutex);
	return(OK);
}

int leave_sync(void ){
	DVKDEBUG(INTERNAL,"\n");
	mutex_unlock(&rt_thread_mutex);
	mutex_lock(&rt_main_mutex);
	return(OK);
}

int leave_thread(void )
{
	DVKDEBUG(INTERNAL,"\n");
	mutex_unlock(&rt_thread_mutex);
	mutex_unlock(&rt_main_mutex);
	return(OK);
}

/*===========================================================================*
 *				rtap_kcleanup				     *
 *===========================================================================*/
void rtap_kcleanup(void)
{
	int rcode;
	DVKDEBUG(INTERNAL,"\n");
	rcode = dvk_unbind(sw_ptr->sw_dcid, rt_ptr->rt_rmt_cep);
	if( rcode < 0) ERROR_PRINT(rcode);
	rcode = dvk_unbind(sw_ptr->sw_dcid, rt_ptr->rt_rmt_sep);
	if( rcode < 0) ERROR_PRINT(rcode);
}

/*===========================================================================*
 *				rt_send_packet				     *
 *===========================================================================*/
int rt_send_packet(void *packet, int len)
{
	int rcode, svr_ep;
	message *m_ptr;
	packet_t *pkt_ptr;

	DVKDEBUG(INTERNAL,"len=%d rt_rmt_idx=%d\n",len, rt_ptr->rt_rmt_idx);
		
	if( rt_ptr->rt_rmt_idx < 0) {
		ERROR_RETURN(EDVSBADF);
	}
	
	m_ptr 			= &sndr_msg;
	m_ptr->m_type 	= REQ_RT_WRITE;
	m_ptr->m3_p1	= packet;
	m_ptr->m3_i2	= len;
	
    DVKDEBUG(INTERNAL,RT_FORMAT, RT_FIELDS(rt_ptr));
	pkt_ptr= ( packet_t *) &packet;
	DVKDEBUG(INTERNAL,"REQ_RT_WRITE: " PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
	DVKDEBUG(INTERNAL,"REQ_RT_WRITE: " PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));

	m_ptr->m3_i1 = rt_ptr->rt_rmt_idx;
	DVKDEBUG(INTERNAL,"SEND PACKET REQUEST: rt_rmt_sep=%d " 
				MSG3_FORMAT, rt_ptr->rt_rmt_sep, MSG3_FIELDS(m_ptr));

	rcode = os_sendrec_T(rt_ptr->rt_rmt_sep, m_ptr, TIMEOUT_MOLCALL);	
	DVKDEBUG(INTERNAL,"rcode=%d\n", rcode);
	if( rcode < 0) {
		ERROR_RETURN(rcode);
	}
	DVKDEBUG(INTERNAL,"RECEIVED SEND PACKET REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	if( m_ptr->m_type < 0) {
		ERROR_RETURN(m_ptr->m_type);
	}
	return(m_ptr->m_type);
}

/*===========================================================================*
 *				rt_open_rtap				     *
 *===========================================================================*/
int rt_open_rtap(rmttap_t *rt_ptr)
{
	int rcode, svr_ep;
	proc_usr_t *proc_ptr;
	message *m_ptr;
	
    DVKDEBUG(INTERNAL,RT_FORMAT, RT_FIELDS(rt_ptr));

	proc_ptr = &rt_ptr->rt_svr_proc; 
	rcode = dvk_getprocinfo(sw_ptr->sw_dcid, rt_ptr->rt_rmt_sep, proc_ptr);		
	DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
			
	m_ptr = &sndr_msg;
	m_ptr->m_type = REQ_RT_OPEN;
	m_ptr->m3_i1  = rt_ptr->rt_index;
	strncpy(m_ptr->m3_ca1, rt_ptr->rt_tap, M3_STRING-1);
	DVKDEBUG(INTERNAL,"rt_ptr->rt_rmt_sep=%d\n", rt_ptr->rt_rmt_sep);
	DVKDEBUG(INTERNAL,"SEND REQ_RT_OPEN REQUEST: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	rcode = os_sendrec_T(rt_ptr->rt_rmt_sep, m_ptr, TIMEOUT_MOLCALL);
	DVKDEBUG(INTERNAL,"rcode=%d\n", rcode);
	if( rcode < 0) {
		ERROR_RETURN(rcode);
	}
	DVKDEBUG(INTERNAL,"RCVD REQ_RT_OPEN REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
	if( m_ptr->m_type < 0) {
		ERROR_RETURN(m_ptr->m_type);
	}
	rt_ptr->rt_rmt_idx	 = m_ptr->m_type; 
	DVKDEBUG(INTERNAL, "Remote TAP %s successfully opened on endpoint %d with remote index %d\n", 
		rt_ptr->rt_tap, rt_ptr->rt_rmt_sep, rt_ptr->rt_rmt_idx);
	
	return(m_ptr->m_type);
}

/*===========================================================================*
 *				try_rtap_open		     
 *===========================================================================*/	
int try_rtap_open(void)
{
	int i, rcode;

	rcode = rt_open_rtap(rt_ptr);  
	if( rcode < 0) {
		ERROR_PRINT(rcode);
		return(rcode);
	}
	return(OK);
}
/*===========================================================================*
 *				init_send_kthread		     
 *===========================================================================*/	
int init_send_kthread(void *arg)
{
	int rcode, ret; 
	proc_usr_t *proc_ptr;
	packet_t *pkt_ptr;
    int len;
	
//	os_fix_helper_signals();
	rcode = rtap_block_signals();
	if( rcode) ERROR_RETURN(rcode);

	rcode = enter_sync();
	if( rcode) ERROR_RETURN(rcode);
	
	sw_ptr->sw_send_tid = os_gettid();
	DVKDEBUG(INTERNAL,"SENDER THREAD: sw_send_ep=%d sw_send_tid=%d\n", 
		sw_ptr->sw_send_ep, sw_ptr->sw_send_tid);

	rcode = dvk_umbind(sw_ptr->sw_dcid, sw_ptr->sw_send_tid , sw_ptr->sw_send_ep);
	if( rcode != sw_ptr->sw_send_ep) {
		ret = leave_thread();
		if(ret) ERROR_PRINT(rcode);
		ERROR_RETURN(rcode);
	}
	
	proc_ptr = &sw_ptr->sw_send_proc;  
    rcode = dvk_getprocinfo(sw_ptr->sw_dcid, sw_ptr->sw_send_ep, proc_ptr);	
    DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));

	// get info and bind remote TAP server endpoint 
	proc_ptr = &rt_ptr->rt_svr_proc; 
	rcode  = dvk_getprocinfo(sw_ptr->sw_dcid, rt_ptr->rt_rmt_sep, proc_ptr);
	if( rcode < 0) {
		ERROR_PRINT(rcode);
	} else {
		DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
		if(test_bit(BIT_SLOT_FREE, &proc_ptr->p_rts_flags)) {
			rcode = dvk_rmtbind(sw_ptr->sw_dcid,"rtap_server",
					rt_ptr->rt_rmt_sep,rt_ptr->rt_nodeid);
			if(rcode < 0) ERROR_PRINT(rcode);
		}
		rcode  = dvk_getprocinfo(sw_ptr->sw_dcid, rt_ptr->rt_rmt_sep, proc_ptr);
		if(rcode < 0) ERROR_PRINT(rcode);
		   DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));		
	}
	ret = leave_thread();
	if(ret) ERROR_PRINT(rcode);
	return(rcode); 
}

int sw_accept_connection(int fd)
{
  int rcode;
  int new;

  DVKDEBUG(INTERNAL,"fd=%d\n", fd);

  new = os_accept_connection(fd);
  if(new < 0){
    ERROR_RETURN(new);
  }
  if((rcode =os_set_fd_block(new, 0)) < 0 ){ // 0 means O_NONBLOCK
    os_close_file(new);
    ERROR_RETURN(rcode);
  }
  return(new);
}

/*===========================================================================*
 *				send_kthread		     
 *===========================================================================*/	
void *send_kthread(void *arg)
{
	int rcode; 
	proc_usr_t *proc_ptr;
	packet_t *pkt_ptr;
    int len;
		
	DVKDEBUG(INTERNAL,"send_kthread\n");

	rcode = try_rtap_open();
	pkt_ptr = &rt_ptr->rt_packet;
	while(1){
		while( rt_ptr->rt_rmt_idx == (-1) ) { // not open 
			DVKDEBUG(INTERNAL,"waiting to retry open\n");
			// os_idle_sleep(WAIT10SECS);
			os_sleep_secs(30);
			DVKDEBUG(INTERNAL,"try_rtap_open\n");
			rcode = try_rtap_open();
		}
		while(1){

			// read from PIPE
			len = net_recvfrom(rt_ptr->rt_kernel_fd, pkt_ptr, sizeof(packet_t));
			if( len < sizeof(ETH_HDR_LEN) || len > sizeof(packet_t)){
				printk("send_kthread - net_recvfrom failed, fd=%d, err=%d\n", 
					rt_ptr->rt_kernel_fd, len);
				ERROR_PRINT(len);
				continue;
			}
			
			DVKDEBUG(INTERNAL,PKTSRC_FORMAT,PKTSRC_FIELDS(pkt_ptr));
			DVKDEBUG(INTERNAL,PKTDST_FORMAT,PKTDST_FIELDS(pkt_ptr));

			// Check if the remote TAP is OPEN 		 
			if( rt_ptr->rt_rmt_idx < 0){
					rcode = try_rtap_open();
					if( rcode < 0){
						ERROR_PRINT(rcode);
						continue;
					}
			} 
			
			rcode = rt_send_packet(pkt_ptr, len);
			if (rcode < EDVSERRCODE) {
				rt_ptr->rt_rmt_idx = (-1);
				ERROR_PRINT(rcode);
				continue;
			}
			if( rcode < 0){		
				ERROR_PRINT(rcode);
				continue;
			}
		}
	}
}

/*===========================================================================*
 *				init_recv_kthread			     
 *===========================================================================*/	
int  init_recv_kthread(void *arg)
{
	int rcode, i,tap_fd, len, rmt_idx,lcl_idx, ret; 
	proc_usr_t *proc_ptr;
	packet_t *pkt_ptr;
	message *m_ptr;
	sock_data_t data;

//	os_fix_helper_signals();
	rcode = rtap_block_signals();
	if( rcode) ERROR_RETURN(rcode);

	rcode = enter_sync();
	if( rcode) ERROR_RETURN(rcode);
	
	sw_ptr->sw_recv_tid = os_gettid();
	DVKDEBUG(INTERNAL,"RECEIVER THREAD: sw_recv_ep=%d sw_recv_tid=%d\n", 
		sw_ptr->sw_recv_ep, sw_ptr->sw_recv_tid);

	// Bind Receiver thread 
	rcode = dvk_umbind(sw_ptr->sw_dcid, sw_ptr->sw_recv_tid , sw_ptr->sw_recv_ep);
	if( rcode != sw_ptr->sw_recv_ep) {
		ret = leave_thread();
		if( ret) ERROR_PRINT(ret);
		ERROR_RETURN(rcode);
	}
	proc_ptr = &sw_ptr->sw_recv_proc;
    rcode = dvk_getprocinfo(sw_ptr->sw_dcid, sw_ptr->sw_recv_ep, proc_ptr);	
    DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
	
	// get info and bind remote TAP client endpoint 
	proc_ptr = &rt_ptr->rt_clt_proc; 
	rcode  = dvk_getprocinfo(sw_ptr->sw_dcid, rt_ptr->rt_rmt_cep, proc_ptr);
	if( rcode < 0) {
		ERROR_PRINT(rcode);
	} else {
		DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
		if(test_bit(BIT_SLOT_FREE, &proc_ptr->p_rts_flags)) {
			rcode = dvk_rmtbind(sw_ptr->sw_dcid,"rtap_client",
					rt_ptr->rt_rmt_cep,rt_ptr->rt_nodeid);
			if(rcode < 0) ERROR_PRINT(rcode);
		}
		rcode  = dvk_getprocinfo(sw_ptr->sw_dcid, rt_ptr->rt_rmt_cep, proc_ptr);
		if(rcode < 0) ERROR_PRINT(rcode);
		DVKDEBUG(INTERNAL,PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	
	}
	ret = leave_thread();
	if(ret) ERROR_PRINT(ret);
	return(rcode);
}


/*===========================================================================*
 *				recv_kthread			     
 *===========================================================================*/	
void *recv_kthread(void *arg)
{
	int rcode, i,tap_fd, len, rmt_idx,lcl_idx, n; 
	proc_usr_t *proc_ptr;
	packet_t *pkt_ptr;
	message *m_ptr;
	sock_data_t data;

	DVKDEBUG(INTERNAL,"recv_kthread\n");

	m_ptr = &rcvr_msg;
	pkt_ptr= &sw_ptr->sw_packet;
	while(1){
		rcode = os_receive_T(ANY, m_ptr, TIMEOUT_MOLCALL); 
		if( rcode < 0){
			DVKDEBUG(INTERNAL,"rcode=%d\n", rcode);
			continue;
		} 
		DVKDEBUG(INTERNAL,"RECEIVED REQUEST: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
		if( m_ptr->m_source != rt_ptr->rt_rmt_cep) {
			ERROR_PRINT(EDVSBADSRCDST);
			continue;
		}
		switch( m_ptr->m_type){
			case REQ_SW_WRITE: // the remote TAP device request to write to local  virtual port 	
				lcl_idx = m_ptr->m3_i1;
				len 	= m_ptr->m3_i2;
				DVKDEBUG(INTERNAL,"RECEIVED REQ_SW_WRITE REQUEST lcl_idx=%d len=%d\n",lcl_idx, len);
				// search the port with this socket FD
				if( lcl_idx != 0){ //  lcl_idx=0 is the only valid device 
					DVKDEBUG(INTERNAL,"error=%d\n", EDVSNODEV);
					rcode = EDVSNODEV;
					break;
				}			
				DVKDEBUG(INTERNAL,RT_FORMAT, RT_FIELDS(rt_ptr));
				rcode = dvk_vcopy(m_ptr->m_source, m_ptr->m3_p1, 
								sw_ptr->sw_recv_ep, pkt_ptr, len);
				if( rcode < 0){
					DVKDEBUG(INTERNAL,"rcode=%d\n", rcode);
					break;
				}
				
				DVKDEBUG(INTERNAL,"REQ_SW_WRITE: " PKTDST_FORMAT, PKTDST_FIELDS(pkt_ptr));
				DVKDEBUG(INTERNAL,"REQ_SW_WRITE: " PKTSRC_FORMAT, PKTSRC_FIELDS(pkt_ptr));
				
				n = os_write_file(rt_ptr->rt_thread_fd, pkt_ptr, len);
				DVKDEBUG(INTERNAL,"os_write_file n=%d len=%d\n", n, len);
				if( n < 0 || n != len ) 
					DVKDEBUG(INTERNAL,"os_write_file n=%d\n", n);
				rcode = OK;
				break;
			default:
				DVKDEBUG(INTERNAL,"Invalid Request: %d\n", m_ptr->m_type);
				DVKDEBUG(INTERNAL,"error=%d\n", m_ptr->m_type);
				rcode = EDVSBADREQUEST;
				break;
		}
		m_ptr->m_type  =  rcode;
		DVKDEBUG(INTERNAL,"SEND REPLY: " MSG3_FORMAT, MSG3_FIELDS(m_ptr));
		rcode = os_send_T( m_ptr->m_source, m_ptr, TIMEOUT_MOLCALL);
		if( rcode < 0){
			DVKDEBUG(INTERNAL,"rcode=%d\n", rcode);
			continue;
		} 
	}
}

static void rtap_init(struct net_device *dev, void *data)
{
	int rcode, i;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *proc_ptr;  
	struct uml_net_private *pri;
	struct rtap_data *dpri;
	struct rtap_init_s *init = data;
	unsigned long stack;
	int err, dvk_pid;

	sw_ptr = &rt_switch;
//	rt_ptr = &rtap;

	pri = netdev_priv(dev);
	dpri = (struct rtap_data *) pri->user;
	dpri->sock_type = init->sock_type;
	dpri->ctl_sock = init->ctl_sock;
	dpri->fd = -1;
	dpri->control = -1;
	dpri->dev = dev;
	/* We will free this pointer. If it contains crap we're burned. */
	dpri->ctl_addr = NULL;
	dpri->data_addr = NULL;
	dpri->local_addr = NULL;

	rtap.rt_rd = dpri;
	printk("rtap backend - %s:%s\n", dpri->sock_type, dpri->ctl_sock);

	DVKDEBUG(INTERNAL,"local_nodeid=%d\n",local_nodeid);
	DVKDEBUG(INTERNAL,"dcid=%d\n",dcid);	
	dvsu_ptr 	= &dvs;
	dcu_ptr 	= &dcu;
	if( local_nodeid == DVS_NO_INIT){
		local_nodeid= dvk_getdvsinfo(dvsu_ptr);
		rcode 		= dvk_getdcinfo(dcid, dcu_ptr);
	}
	if( local_nodeid < 0){
		printk(KERN_ERR "dvk_getdvsinfo failed local_nodeid=%d\n", local_nodeid);
		return;
	} else {
		DVKDEBUG(INTERNAL,"local_nodeid=%d\n",local_nodeid);
		DVKDEBUG(INTERNAL, DVS_USR_FORMAT, DVS_USR_FIELDS(dvsu_ptr));
	}
	if(rcode < 0) {
		printk(KERN_ERR "dvk_getdcinfo failed rcode=%d\n", rcode);
		return;
	} else {
		sw_ptr->sw_dcid = dcid;
		sw_ptr->sw_name = dcu_ptr->dc_name;
		DVKDEBUG(INTERNAL, DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));	
		DVKDEBUG(INTERNAL, DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));
	}
	
	// Local Endpoint has is calculated from the first Client Endpoint plus the index 
	sw_ptr->sw_send_ep = (dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks);
	sw_ptr->sw_send_ep += (local_nodeid);
	sw_ptr->sw_recv_ep = (local_nodeid);

	rt_ptr->rt_index = 0;  // the only rtap interface 
   	rt_ptr->rt_rmt_sep = rt_ptr->rt_nodeid;
	rt_ptr->rt_rmt_idx = (-1);
	rt_ptr->rt_rmt_cep = (dcu_ptr->dc_nr_sysprocs - dcu_ptr->dc_nr_tasks);
	rt_ptr->rt_rmt_cep += (rt_ptr->rt_nodeid);

	stack = alloc_stack(0, 0);
	rcode = start_sw_threads(stack + PAGE_SIZE - sizeof(void *), NULL);
	if(rcode < 0) {
		printk(KERN_ERR "Failed to start pseudo-switch threads (rcode = %d) - ", rcode);
		return;
	} 
		
	DVKDEBUG(INTERNAL, "sw_send_tid=%d\n", sw_ptr->sw_send_tid);
	proc_ptr = &sw_ptr->sw_send_proc;
	rcode = dvk_getprocinfo(dcid, sw_ptr->sw_send_ep, proc_ptr);
	if(rcode < 0) {
		printk(KERN_ERR "dvk_getprocinfo failed rcode=%d\n", rcode);
		return;
	}else{
		DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
	}
	
	DVKDEBUG(INTERNAL, "sw_recv_tid=%d\n", sw_ptr->sw_recv_tid);
	proc_ptr = &sw_ptr->sw_recv_proc;
	rcode = dvk_getprocinfo(dcid, sw_ptr->sw_recv_ep, proc_ptr);
	if(rcode < 0) {
		printk(KERN_ERR "dvk_getprocinfo failed rcode=%d\n", rcode);
		return;
	}else{
		DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));
	}
	
	DVKDEBUG(INTERNAL, RT_FORMAT, RT_FIELDS(rt_ptr));
 	
	DVKDEBUG(INTERNAL, SW_FORMAT, SW_FIELDS(sw_ptr));	
}

static int rtap_read(int fd, struct sk_buff *skb, struct uml_net_private *lp)
{
	return net_read(fd, skb_mac_header(skb),
			    skb->dev->mtu + ETH_HEADER_OTHER);
}

static int rtap_write(int fd, struct sk_buff *skb, struct uml_net_private *lp)
{
	return rtap_user_write(fd, skb->data, skb->len,
				 (struct rtap_data *) &lp->user);
}

static const struct net_kern_info rtap_kern_info = {
	.init			= rtap_init,
	.protocol		= eth_protocol,
	.read			= rtap_read,
	.write			= rtap_write,
};

static int rtap_setup(char *str, char **mac_out, void *data)
{
	int rcode;
	struct rtap_init_s *init = data;
	char *remain;
	char *node_str;
	char *tap_str;
	
	DVKDEBUG(INTERNAL,"if_spec=%s dcid=%d\n", str, dcid);

// 		boot line format: eth0:rtap,<MAC_address>,<rtap_name>,<rmt_nodeid>
	remain = split_if_spec(str, mac_out, &tap_str, &node_str, NULL);
	if (remain != NULL)
		printk(KERN_WARNING "rtap_setup: Ignoring data socket specification\n");

	DVKDEBUG(INTERNAL,"mac_out=%s tap_str=%s node_str=%s\n", *mac_out, tap_str, node_str);
	
#define BASE10	10	
	rt_ptr = &rtap;
	rt_ptr->rt_nodeid 	= simple_strtol(node_str, &remain, BASE10);
	rt_ptr->rt_tap 		= tap_str;
	rt_ptr->rt_mac 		= *mac_out;
	DVKDEBUG(INTERNAL,"rt_tap=%s rt_mac=%s rt_nodeid=%d\n",  
			rt_ptr->rt_tap, rt_ptr->rt_mac , rt_ptr->rt_nodeid);

	strncpy(rt_ptr->rt_sock_type, "unix", MAXSOCKNAME-1);
	sprintf(rt_ptr->rt_ctrl_path,"/tmp/rtap%02d.ctl",dcid);  
	init->sock_type = rt_ptr->rt_sock_type;
	init->ctl_sock  = rt_ptr->rt_ctrl_path;
 
 	DVKDEBUG(INTERNAL,RT_FORMAT, RT_FIELDS(rt_ptr));
	DVKDEBUG(INTERNAL,RT2_FORMAT, RT2_FIELDS(rt_ptr));

 	rcode = dvk_open();
	if( rcode < 0) {
		printk(KERN_ERR "dvk_open failed rcode=%d\n", rcode);
	}
	DVKDEBUG(INTERNAL,"dvk_open=%d\n", rcode);
 
	return 1;
}

static struct transport rtap_transport = {
	.list 		= LIST_HEAD_INIT(rtap_transport.list),
	.name 		= "rtap",
	.setup  	= rtap_setup,
	.user 		= &rtap_user_info,
	.kern 		= &rtap_kern_info,
	.private_size 	= sizeof(struct rtap_data),
	.setup_size 	= sizeof(struct rtap_init_s),
};

static int register_rtap(void)
{
	register_transport(&rtap_transport);
	return 0;
}

late_initcall(register_rtap);

// #endif  //  CONFIG_UML_NET_RTAP 
