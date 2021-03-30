/*
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#ifndef __RTAP_H__
#define __RTAP_H__

#define		RTAP_UNIX		0
#define		RTAP_PIPE		1

#define		RTAP_IPC		RTAP_PIPE

#pragma message ("RTAP_IPC=" STRINGIFY(RTAP_IPC))

#if RTAP_IPC == RTAP_PIPE
#define PIPE_READ		0
#define PIPE_WRITE		1
#endif //  RTAP_IPC


#include "um_dvk.h"
#include "glo_dvk.h"
#include "usr_dvk.h"

#define SWITCH_VERSION 3
#define LISTEN_BACKLOG 	15
#define ETH_ALEN 		6
#define ETH_HDR_LEN		((ETH_ALEN*2)+2)
#define NR_RMTTAP		NR_NODES
#define NO_DCID 		PROC_NO_PID	
#define YES				1
#define NO				0
#define	MAXSOCKNAME		16
#define	MAXSOCKPATH		64
#define POLL_TIMEOUT 	(30 * 1000)
#define SWITCH_MAGIC 0xfeedface

enum request_type { REQ_NEW_CONTROL,
					REQ_RT_OPEN,
					REQ_RT_WRITE,
					REQ_RT_CLOSE,
					REQ_SW_WRITE};
					
struct rtap_data {
	char *sock_type;
	char *ctl_sock;
	void *ctl_addr;
	void *data_addr;
	void *local_addr;
	int fd;
	int control;
	void *dev;
};

typedef struct rtap_data rtap_data_t;
#define RTD_FORMAT "sock_type=%s ctl_sock=%s fd=%d control=%d\n"
#define RTD_FIELDS(p) p->sock_type, p->ctl_sock, p->fd, p->control

struct  packet_s {
  struct {
    unsigned char dest[ETH_ALEN];
    unsigned char src[ETH_ALEN];
    unsigned char proto[2];
  } rt_hdr;
  unsigned char data[1500];
} ;
typedef struct packet_s packet_t;

#define PKTSRC_FORMAT "Src: %02x:%02x:%02x:%02x:%02x:%02x proto:%04x\n"
#define PKTSRC_FIELDS(p) p->rt_hdr.src[0], p->rt_hdr.src[1], p->rt_hdr.src[2], p->rt_hdr.src[3], p->rt_hdr.src[4], p->rt_hdr.src[5], p->rt_hdr.proto
#define PKTDST_FORMAT "Dst: %02x:%02x:%02x:%02x:%02x:%02x\n"
#define PKTDST_FIELDS(p) p->rt_hdr.dest[0], p->rt_hdr.dest[1], p->rt_hdr.dest[2], p->rt_hdr.dest[3], p->rt_hdr.dest[4], p->rt_hdr.dest[5] 

struct switch_s{
	int		sw_recv_ep;					// receiver thread endpoint (server)
	int		sw_send_ep;					// sender thread endpoint (client)
	int		sw_dcid;					// DC ID
#if RTAP_IPC == RTAP_PIPE
	int		sw_Sfdes[2];
	int		sw_Rfdes[2];
#else //  RTAP_IPC
	int		sw_tap_fd;					// tap FD
	int		sw_svr_fd;					// connect FD
	int		sw_clt_fd;					// client FD
	char 	*sw_sock_type;
	char 	*sw_svr_sock;
#endif // RTAP_IPC
	char 	*sw_name; 					// switch name 
    pid_t	sw_recv_tid;				// receiver thread TID 
    pid_t	sw_send_tid;				// sender  thread TID 
	proc_usr_t sw_send_proc;			// sender  thread process descriptor
	proc_usr_t sw_recv_proc;			// receiver thread process descriptor
	char	sw_ctrl_path[MAXSOCKPATH];		// control path name 
	packet_t	sw_packet;
};
typedef struct switch_s switch_t;

#if RTAP_IPC == RTAP_PIPE
#define SW_FORMAT "sw_recv_ep=%d sw_send_ep=%d sw_dcid=%d sw_Sfdes[1]=%d sw_Rfdes[0]=%d sw_recv_tid=%d sw_send_tid=%d  sw_name=%s\n"
#define SW_FIELDS(p) p->sw_recv_ep, p->sw_send_ep, p->sw_dcid, p->sw_Sfdes[1], p->sw_Rfdes[0] ,p->sw_recv_tid, p->sw_send_tid, p->sw_name
#else //  RTAP_IPC
#define SW_FORMAT "sw_recv_ep=%d sw_send_ep=%d sw_dcid=%d sw_svr_fd=%d sw_clt_fd=%d sw_recv_tid=%d sw_send_tid=%d  sw_name=%s\n"
#define SW_FIELDS(p) p->sw_recv_ep, p->sw_send_ep, p->sw_dcid, p->sw_svr_fd, p->sw_clt_fd,p->sw_recv_tid, p->sw_send_tid, p->sw_name
#endif // RTAP_IPC

struct sock_data {
  int fd;
//  struct sockaddr_un sock;
};
typedef struct sock_data sock_data_t;

struct rmttap_s{
	int		rt_index; 						// index in the array of local structures 
	int		rt_nodeid;						// in which node is this TAP 
#if RTAP_IPC == RTAP_UNIX
	int		rt_clt_fd;	
	int		rt_svr_fd;
#endif // RTAP_IPC
	int		rt_rmt_idx;
	int		rt_rmt_cep;						// remote CLIENT endpoint  	
	int		rt_rmt_sep;						// remote SERVER endpoint who opens this TAP 
	int		rt_rmttap_fd;					// rmttap fd  in the remote node or local fd for remote switch  
	char 	*rt_tap;						// name of the remote TAP device
//	char 	*rt_name;						// reference name in the local switch of the TAP device 
//	struct sockaddr_un rt_ctrl_sun;			// control unix socket to the switch 
	proc_usr_t 	rt_svr_proc;				// remote switch server process descriptor 
	proc_usr_t 	rt_clt_proc;				// remote switch client process descriptor 
//	struct request_v3 rt_req;				// request to the local switch 
	rtap_data_t *rt_rd;					// 
	char 	*rt_mac;
	char	rt_sock_type[MAXSOCKNAME];		// socket type "unix"
	char	rt_ctrl_path[MAXSOCKPATH];		// control path name 
	char	rt_data_path[MAXSOCKPATH];		// data path name 
	packet_t rt_packet;
};
typedef struct rmttap_s rmttap_t;

#if RTAP_IPC == RTAP_PIPE
#define RT_FORMAT "rt_index=%d rt_nodeid=%d rt_tap=%s rt_rmt_cep=%d rt_rmt_sep=%d rt_rmt_idx=%d rt_rmttap_fd=%d\n"
#define RT_FIELDS(p) p->rt_index, p->rt_nodeid, p->rt_tap, p->rt_rmt_cep, p->rt_rmt_sep, p->rt_rmt_idx, p->rt_rmttap_fd
#else // RTAP_IPC
#define RT_FORMAT "rt_index=%d rt_nodeid=%d rt_tap=%s rt_clt_fd=%d rt_svr_fd=%d rt_rmt_cep=%d rt_rmt_sep=%d rt_rmt_idx=%d rt_rmttap_fd=%d\n"
#define RT_FIELDS(p) p->rt_index, p->rt_nodeid, p->rt_tap, p->rt_clt_fd, p->rt_svr_fd, p->rt_rmt_cep, p->rt_rmt_sep, p->rt_rmt_idx, p->rt_rmttap_fd
#endif // RTAP_IPC
#define RT2_FORMAT "rt_index=%d rt_nodeid=%d rt_tap=%s rt_mac=%s\n"
#define RT2_FIELDS(p) p->rt_index, p->rt_nodeid, p->rt_tap, p->rt_mac

struct rtap_init_s {
	char *sock_type;
	char *ctl_sock;
};
typedef struct rtap_init_s rtap_init_t;

int rtap_user_write(int fd, void *buf, int len, struct rtap_data *pri);
int rtap_user_read(int fd, void *buf, int len);
int start_sw_threads(unsigned long sp);
void *send_kthread(void *arg);
void *recv_kthread(void *arg);
int init_send_kthread(void *arg);
int init_recv_kthread(void *arg);
int init_sync(void );
int end_sync(void );
int enter_sync(void );
int leave_sync(void );
int leave_thread(void );
int rtap_block_signals(void);
unsigned int os_sleep_secs(unsigned int seconds);
long os_receive_T(int src, message *m_ptr, long int timeout);
long os_send_T( int dst, message *m_ptr, long int timeout);
long os_sendrec_T(int dst, message *m_ptr, long int timeout);
int rtap_receiver_kinit(void);
int rtap_sender_kinit( void);
int  rtap_server_wait( void);

void rtap_kcleanup(void);

#include "rtap_glo.h"

#endif
