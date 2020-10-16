/* Copyright 2002 Jeff Dike
 * Licensed under the GPL
 */

#ifndef __SWITCH_H__
#define __SWITCH_H__

#define SWITCH_VERSION 3
#define ETH_ALEN 		6
#define LISTEN_BACKLOG 	15
#define NR_RMTTAP		NR_NODES
#define NO_DCID 	PROC_NO_PID	
#define YES				1
#define NO				0
#define	MAXSOCKNAME		16
#define	MAXSOCKPATH		64
#define POLL_TIMEOUT 	(30 * 1000)

#include <assert.h>

#include <asm/ptrace.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <assert.h>
#ifdef notdef
#include <stddef.h>
#endif

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/syscall.h>    /* For SYS_xxx definitions */
#include <sys/un.h>
#include <sys/poll.h>

#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <net/if.h>
#include <net/if_arp.h>   
#include <arpa/inet.h>
#include <linux/if_tun.h>

#define DVS_USERSPACE	1
#define _GNU_SOURCE
//#define  __USE_GNU
#include <sched.h>
#define cpumask_t cpu_set_t

#include "../../include/com/dvs_config.h"
#include "../../include/com/config.h"
#include "../../include/com/com.h"
#include "../../include/com/const.h"
#include "../../include/com/cmd.h"
#include "../../include/com/priv_usr.h"
//#include "../../include/com/priv.h"
#include "../../include/com/dc_usr.h"
#include "../../include/com/node_usr.h"
#include "../../include/com/proc_sts.h"
#include "../../include/com/proc_usr.h"
//#include "../../include/com/proc.h"
#include "../../include/com/proxy_sts.h"
#include "../../include/com/proxy_usr.h"
#include "../../include/com/dvs_usr.h"
#include "../../include/com/dvk_calls.h"
#include "../../include/com/dvk_ioctl.h"
#include "../../include/com/dvs_errno.h"
#include "../../include/dvk/dvk_ioparm.h"
#include "../../include/com/stub_dvkcall.h"


enum request_type { REQ_NEW_CONTROL,
					REQ_RT_OPEN,
					REQ_RT_WRITE,
					REQ_RT_CLOSE};
				
struct request_v0 {
enum request_type type;
	union {
		struct {
			unsigned char addr[ETH_ALEN];
			struct sockaddr_un name;
		} new_control;
	} u;
};

#define SWITCH_MAGIC 0xfeedface

struct request_v1 {
uint32_t magic;
enum request_type type;
		union {
			struct {
				unsigned char addr[ETH_ALEN];
				struct sockaddr_un name;
			} new_control;
		} u;
};

struct request_v2 {
	uint32_t magic;
	uint32_t version;
	enum request_type type;
	struct sockaddr_un sock;
};

struct reply_v2 {
	unsigned char mac[ETH_ALEN];
	struct sockaddr_un sock;
};

struct request_v3 {
	uint32_t magic;
	uint32_t version;
	enum request_type type;
	struct sockaddr_un sock;
};

#define RQ3_FORMAT "magic=%d version=%d type=%d\n"
#define RQ3_FIELDS(p) p->magic, p->version, p->type

union request {
	struct request_v0 v0;
	struct request_v1 v1;
	struct request_v2 v2;
	struct request_v3 v3;
};

struct daemon_data {
	char *sock_type;
	char *ctrl_sock;
	void *ctrl_addr;
	void *data_addr;
	void *local_addr;
	int con_fd;
	int ctrl_fd;
	void *dev;
};
typedef struct daemon_data daemon_data_t;
#define DD_FORMAT "sock_type=%s ctrl_sock=%s con_fd=%d ctrl_fd=%d\n"
#define DD_FIELDS(p) p->sock_type, p->ctrl_sock, p->con_fd, p->ctrl_fd

typedef struct daemon_init daemon_init_t;
#define DI_FORMAT "sock_type=%s ctrl_sock=%s\n"
#define DI_FIELDS(p) p->sock_type, p->ctrl_sock

struct packet {
  struct {
    unsigned char dest[ETH_ALEN];
    unsigned char src[ETH_ALEN];
    unsigned char proto[2];
  } header;
  unsigned char data[1500];
};

struct switch_s{
	int		sw_hub; 					// hub mode ?
	int		sw_daemon; 					// deamon mode ?
	int		sw_recv_ep;					// receiver thread endpoint (server)
	int		sw_send_ep;					// sender thread endpoint (client)
	int		sw_dcid;					// DC ID
	char 	*sw_tap;					//  local TAP name for the switch 
	char 	*sw_ctrl_path;				// switch control socket path 
	char 	*sw_data_path;				// switch data socket path 
	char 	*sw_name; 					// switch name 
    pid_t	sw_recv_tid;				// receiver thread TID 
    pid_t	sw_send_tid;				// sender  thread TID 
	proc_usr_t sw_recv_proc;			// receiver thread process descriptor
	proc_usr_t sw_send_proc;			// sender  thread process descriptor
	pthread_t sw_recv_thread;			// receiver thread 
	pthread_t sw_send_thread;			// sender  thread 
};
typedef struct switch_s switch_t;

#define SW_FORMAT "sw_hub=%d sw_daemon=%d sw_recv_ep=%d sw_send_ep=%d sw_dcid=%d sw_ctrl_path=%s sw_name=%s\n"
#define SW_FIELDS(p) p->sw_hub, p->sw_daemon, p->sw_recv_ep, p->sw_send_ep, p->sw_dcid, p->sw_ctrl_path, p->sw_name

#define SW1_FORMAT "sw_recv_tid=%d sw_send_tid=%d sw_name=%s\n"
#define SW1_FIELDS(p) p->sw_recv_tid, p->sw_send_tid, p->sw_name

struct rmttap_s{
	int		rt_index; 						// index in the array of local structures 
	int		rt_nodeid;						// in which node is this TAP 
	int		rt_ctrl_fd;						// control FD to the local switch 
	int		rt_data_fd;						// data  FD to the local switch 
	int		rt_poll_idx;					// index in the fds array when polling 
	int		rt_rmttap_idx;					// rmttap index  in the remote node 
	char 	*rt_tap;						// name of the remote TAP device
	char 	*rt_name;						// reference name in the local switch of the TAP device 
	struct sockaddr_un rt_ctrl_sun;			// control unix socket to the switch 
	struct sockaddr_un rt_data_sun;			// data unix socket to the switch 
	proc_usr_t 	rt_proc;					// remote switch server process descriptor 
	struct request_v3 rt_req;				// request to the local switch 
	char	rt_sock_type[MAXSOCKNAME];		// socket type "unix"
	char	rt_ctrl_path[MAXSOCKPATH];		// control path name 
	char	rt_data_path[MAXSOCKPATH];		// data path name 
};
typedef struct rmttap_s rmttap_t;

#define RT_FORMAT "rt_index=%d rt_nodeid=%d rt_tap=%s rt_ctrl_fd=%d rt_data_fd=%d rt_poll_idx=%d rt_rmttap_idx=%d rt_name=%s\n"
#define RT_FIELDS(p) p->rt_index, p->rt_nodeid, p->rt_tap, p->rt_ctrl_fd, p->rt_data_fd, p->rt_poll_idx, p->rt_rmttap_idx, p->rt_name

#include "glo.h"
#include "../debug.h"
#include "../macros.h"

#include "port.h"
#include "hash.h"
#ifdef TUNTAP
#include "tuntap.h"
#include "rmttap.h"
#include "daemon.h"
#endif

#endif
