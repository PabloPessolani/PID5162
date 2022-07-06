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

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/syscall.h>    /* For SYS_xxx definitions */
#include <sys/msg.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <net/if_arp.h>   
#include <arpa/inet.h>

#include <lz4frame.h>

#include "dsp_spread.h"
#define PXYDBG 			1	

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
#include "../../include/generic/hello.h"

typedef unsigned long long jiff;
#define BUFF_SIZE		MAXCOPYBUF
#define MAX_MESSLEN     (BUFF_SIZE+1024)
#define MAX_VSSETS      10
#define MAX_MEMBERS     NR_NODES
#define SVR_QUEUEBASE	7000
#define CLT_QUEUEBASE	7100
#define LB_MSGTYPE		1234		
#define LB_TIMEOUT_5SEC	5
#define LB_TIMEOUT_MSEC	0
#define LB_ERROR_SPEEP	5
#define MC_LB_INFO 		0xDA
#define	NR_MAX_CONTROL	32
#define LB_INVALID		(-1) 
#define MAXCMDLEN 		1024
#define MAX_RETRIES		3
#define FD_MAXRETRIES 	5
#define MAX_RANDOM_RETRIES	sizeof(unsigned int)

#define SOCKET_BUFFER_SIZE     65536
#define PING_WAIT_TIME   5
#define PING_NR_PACKETS  3

#define LB_SEND_TIMEOUT		30	
#define LB_RECV_TIMEOUT		30	
#define LB_PERIOD_DEFAULT	30	
#define LB_START_DEFAULT	60	
#define LB_SHUTDOWN_DEFAULT	60
#define LB_VMSTART_TIME 	(2*60)	
#define SECS_BY_HOUR		3600

#define LVL_NOTINIT		(-1) 
#define	LVL_IDLE		0
#define	LVL_BUSY		1
#define	LVL_SATURATED	2

#define	NO		0
#define	YES		1

#define	MAX_SVC_NR	(sizeof(unsigned long int) * 8)
#define LBP_BASE_PORT     		3000
#define	LBP_CLT2SVR				1111
#define	LBP_SVR2CLT				2222
#define	LBP_SVR2SVR				3333
#define	LBP_CLT2CLT				4444
#define	LBP_LB2SVR				5555
#define	PROG_BIND 				MAX_BIND_TYPE

#define PX_INVALID		(-1) 

#define HEADER_SIZE sizeof(proxy_hdr_t)
#define BASE_PORT     		3000

#define PWS_ENABLED			0 			// Proxy Web Server (statistics) ENABLED 
#define BASE_PWS_SPORT 		4000		// SENDER proxy web server base port
#define BASE_PWS_RPORT		5000		// RECEIVER proxy web server base port
#define PWS_BUFSIZE 		65536

// makes remotes clients automatic bind when they send a message to local receiver proxy 
// if local  SYSTASK is running, it does not work 
// it enables automating binding through SYSTASK 
// define SYSTASK_BIND 1

#define RETRY_MS		2000  /* Miliseconds */
#define RETRY_US		2000 /* Microseconds */
#define MAX_RETRIES		5
#define MP_TIMEOUT_5SEC	5
#define MP_TIMEOUT_1SEC	1
#define MP_DFT_PERIOD 	30

#define MP_SEND_TIMEOUT 30
#define MP_RECV_TIMEOUT 30

#define BIND_RETRIES	3
#define BIND_RETRIES	3
#define BATCH_PENDING 	1
#define CMD_PENDING 	1

#define PX_PROTO_NONE     		0
#define PX_PROTO_TCP     		1
#define PX_PROTO_UDP     		2
#define PX_PROTO_TIPC     		3

#define TRUE 1
#define NO     		0
#define YES    		1

typedef struct {
	char *svc_name;				// server name from configuration file 
	int	svc_index;				// service array index
	int	svc_dcid;				// server dcid
	int svc_extep;				// server external endpoint 
	int svc_minep;				// server lower endpoint when execute the server 
	int svc_maxep;				// server higher endpoint when execute the server 
	int svc_bind;				// Which kind of bind must be done on server creation
	char *svc_prog;				// Program to execute on server
    unsigned int svc_bm_params;	// bitmap of Config Service Parameters

}service_t;
#define SERVICE_FORMAT 	   "svc_name=%s svc_dcid=%d svc_extep=%d svc_minep=%d svc_maxep=%d svc_bind=%X svc_prog=%s\n"
#define SERVICE_FIELDS(p)  p->svc_name, p->svc_dcid, p->svc_extep, p->svc_minep, p->svc_maxep, p->svc_bind, p->svc_prog

typedef struct {
    int 			se_dcid;
    int				se_clt_nodeid;
	int				se_clt_ep;
	int				se_clt_PID;
	
	int				se_lbclt_ep;
	int				se_lbsvr_ep;
	
    int				se_svr_nodeid;
	int				se_svr_ep;
	int				se_svr_PID;	
	char 			*se_rmtcmd;
	service_t		*se_service;
	
} sess_entry_t;

#define SESE_LB_FORMAT 	"se_dcid=%d se_lbclt_ep=%d se_lbsvr_ep=%d \n"
#define SESE_LB_FIELDS(p) p->se_dcid, p->se_lbclt_ep, p->se_lbsvr_ep

#define SESE_CLT_FORMAT 	"se_dcid=%d se_clt_nodeid=%d se_clt_ep=%d se_clt_PID=%d\n"
#define SESE_CLT_FIELDS(p) p->se_dcid, p->se_clt_nodeid, p->se_clt_ep, p->se_clt_PID

#define SESE_SVR_FORMAT 	"se_dcid=%d se_svr_nodeid=%d se_svr_ep=%d se_svr_PID=%d\n"
#define SESE_SVR_FIELDS(p) p->se_dcid, p->se_svr_nodeid, p->se_svr_ep, p->se_svr_PID

// Load Balancer Proxy Descriptor
struct lbpx_desc_s {
    pthread_t 		lbp_thread;
	proxy_hdr_t 	*lbp_header;
	proxy_payload_t *lbp_payload;	/* uncompressed payload 		*/	

	int				lbp_sd; 		// Sender TCP socket descriptor 
	int				lbp_lsd; 		// Receiver TCP LISTEN socket descriptor 
	int				lbp_csd; 		// Sender TCP CONNECTION socket descriptor 
	
	int				lbp_port;
	struct sockaddr_in lbp_rmtclt_addr;
	struct sockaddr_in lbp_rmtsvr_addr;
#define lbp_lclsvr_addr lbp_rmtsvr_addr	
	struct hostent *lbp_rmthost;
	long			lbp_msg_ok;
	long			lbp_msg_err;
};
typedef struct lbpx_desc_s lbpx_desc_t;

#define CMD_TSFORMAT 		"c_timestamp=%lld.%.9ld\n" 
#define CMD_TSFIELDS(p) 	(long long) p->c_timestamp.tv_sec, p->c_timestamp.tv_nsec
#define CMD_PIDFORMAT 		"c_flags=0x%lX c_pid=%ld\n" 
#define CMD_PIDFIELDS(p) 	p->c_flags, p->c_pid

struct client_s {
	char *clt_name;
	int	clt_index;				// client  array index
	int	clt_nodeid;
	int	clt_rport;				// Client Receiver port 
	int clt_sport;				// Client Receiver port 
	int	clt_compress;			// Enable LZ4 Compression 0:NO 1:YES
	int	clt_batch;				// Enable message batching 0:NO 1:YES
	
	unsigned long int clt_px_sts; 	
    unsigned int	  clt_bm_params;	// bitmap of Config Client Parameters
	struct timespec   clt_last_cmd; 	// timestamp of the last cmd received from the client

	LZ4F_errorCode_t clt_lz4err;
	size_t			clt_offset;
	size_t			clt_maxCsize;		/* Maximum Compressed size */
	size_t			clt_maxRsize;		/* Maximum Raw size		 */
	__attribute__((packed, aligned(4)))
	LZ4F_compressionContext_t 	clt_lz4Cctx __attribute__((aligned(8))); /* Compression context */
	LZ4F_decompressionContext_t clt_lz4Dctx __attribute__((aligned(8))); /* Decompression context */
	
	lbpx_desc_t clt_spx;
	lbpx_desc_t clt_rpx;

	pthread_mutex_t clt_agent_mtx; // controls when a server process is started or killed  
	pthread_cond_t  clt_agent_cond;  

	pthread_mutex_t clt_node_mtx;  // controls when a complete server node VM is started or stopped 
	pthread_cond_t  clt_node_cond;  

    TAILQ_ENTRY(client_s) clt_tail_entry;         /* Tail queue. */

} ;
typedef struct client_s client_t;

#define CLIENT_FORMAT 	"clt_name=%s clt_nodeid=%d clt_rport=%d clt_sport=%d clt_compress=%d clt_batch=%d\n"
#define CLIENT_FIELDS(p)  p->clt_name, p->clt_nodeid, p->clt_rport, p->clt_sport,  p->clt_compress, p->clt_batch 

struct server_s{
	char *svr_name;			// server name from configuration file 
	int	svr_index;			// server  array index
	int	svr_nodeid;			// server nodeid
	int svr_rport;			// server Receiver port 
	int svr_sport;			// Server Sender port 
	int	svr_level;			// Load LEVEL 		
    int	svr_load;			// CPU Load (0-100) . Value (-1) implies INVALID
	int	svr_compress;		// Enable LZ4 Compression 0:NO 1:YES
	int	svr_batch;			// Enable message batching 0:NO 1:YES
	
	int svr_icmp_fd;		// used by FD
	int svr_icmp_sent;		// used by FD
	int svr_icmp_rcvd;		// used by FD
	int svr_icmp_seq; 		// used by FD
	int svr_icmp_retry;		// used by FD
	int svr_max_retries;
	struct timeval svr_icmp_ts;	// used by FD 
	struct sockaddr_in svr_dstaddr; // used by FD
	struct sockaddr_in svr_from;
	
    unsigned int svr_bm_params;	// bitmap of Config Server Parameters

	struct timespec svr_idle_ts; /* last timestamp	with the server not UNLOADED 											*/
	struct timespec svr_ss_ts;	 /* Start/Stop VM timestamp */
	struct timespec svr_last_cmd; /* timestamp of the last cmd received from the server */

	int	svr_idle_count;		// Number o Idle cycles that the server has  // NOT USED YET  

	char *svr_image;		// string to command which START the server NODE 
	
	unsigned long int svr_px_sts;	// server proxy status  	
	unsigned long int svr_bm_sts; 	
	unsigned long int svr_bm_svc; 	// bitmap of service endpoint used

	char svr_sendpkt[SOCKET_BUFFER_SIZE];
	char svr_recvpkt[SOCKET_BUFFER_SIZE];

	lbpx_desc_t svr_spx;		// Server sender proxy 
	lbpx_desc_t svr_rpx;		// Server receiver proxy	

	LZ4F_errorCode_t svr_lz4err;
	size_t			svr_offset;
	size_t			svr_maxCsize;		/* Maximum Compressed size */
	size_t			svr_maxRsize;		/* Maximum Raw size		 */
	__attribute__((packed, aligned(4)))
	LZ4F_compressionContext_t 	svr_lz4Cctx __attribute__((aligned(8))); /* Compression context */
	LZ4F_decompressionContext_t svr_lz4Dctx __attribute__((aligned(8))); /* Decompression context */
		
	pthread_mutex_t svr_mutex; // protect on change of status and load level.

#ifdef SPREAD_MONITOR
	pthread_mutex_t svr_agent_mtx;  // controls when a server process is started or killed 
	pthread_cond_t  svr_agent_cond;  
#endif // SPREAD_MONITOR

	pthread_mutex_t svr_conn_mtx;  
	pthread_cond_t  svr_conn_scond;
	pthread_cond_t  svr_conn_rcond;	

	pthread_mutex_t svr_node_mtx;  // controls when a complete server node VM is started or stopped 
	pthread_cond_t  svr_node_cond;  
	
    TAILQ_ENTRY(server_s) svr_tail_entry;         /* Tail queue. */

	TAILQ_HEAD(svr_clthead,client_s) svr_clt_head;
	TAILQ_HEAD(svr_svrhead,server_s) svr_svr_head;
	
	pthread_mutex_t svr_tail_mtx;  		// to protect the linked list
	
};
typedef struct server_s server_t;

#define SERVER_FORMAT 	"svr_name=%s svr_nodeid=%d svr_rport=%d svr_sport=%d svr_level=%d svr_load=%d svr_bm_svc=%lX\n"
#define SERVER_FIELDS(p) p->svr_name, p->svr_nodeid, p->svr_rport, p->svr_sport, p->svr_level, p->svr_load,  p->svr_bm_svc 
#define SERVER1_FORMAT 	"svr_name=%s svr_nodeid=%d svr_compress=%d svr_batch=%d\n"
#define SERVER1_FIELDS(p) p->svr_name, p->svr_nodeid, p->svr_compress, p->svr_batch 

#define	MAX_MEMBER_NAME		64

typedef struct {
	char 	*lb_name;
	int		lb_nodeid;
	int		lb_pid;
	
	int		lb_lowwater; 		// low water load (0-100)
	int		lb_highwater;		// low water load (0-100)
	int		lb_period;			// load measurement period in seconds (1-3600)
	int		lb_start;			// period to wait to start a server node VM in seconds (1-3600)
	int		lb_stop;			//  period to wait to shutdown  a server node VM in seconds (1-3600)
	int		lb_min_servers; 	// minimum count of nodes  (0-dvs_ptr->d_nr_nodes)
	int		lb_max_servers;		// maximum count of nodes (0-dvs_ptr->d_nr_nodes)
	int 	lb_compress_opt;

	int		lb_hellotime;		// time between HELLO

	char 	*lb_vm_start;		// string to command which START the server VM 
	char 	*lb_vm_stop;		// string to command which STOP the server VM 
	char 	*lb_vm_status;		// string to command which get the server's VM STATUS
	
	char 	*lb_cltname;		// hostname of this LB inside CLIENT NETWORK
	char 	*lb_svrname;		// hostname of this LB inside SERVER NETWORK
	char 	*lb_cltdev;			// device name of this LB inside CLIENT NETWORK i.e eth1
	char 	*lb_svrdev;			// device name of this LB inside SERVER NETWORK  i.e eth0

	char 	*lb_ssh_host;		// Hostname of the Hypervisor 
	char 	*lb_ssh_user;		// User to use in SSH session with the Hypervisor 
	char 	*lb_ssh_pass;		// Password to use in SSH session with the Hypervisor 


    unsigned int	lb_bm_params;	// bitmap of Config Load Balancer Parameters
    unsigned int	lb_bm_nodes;	// NOT USED bitmap of Configured nodes
	int				lb_nr_nodes;	// NOT USED number of configured nodes 

// START MULTIPLE ACCESS FIELDS 
    unsigned int	lb_bm_active;	// bitmap of Connected nodes (reacheable by ping)
    unsigned int	lb_bm_proxy;	// bitmap of Connected proxies 
    unsigned int	lb_bm_init;		// bitmap  initialized nodes 
    unsigned int	lb_bm_tested;	// bitmap which denotes that the node not be tested by the Failure Detector. 
	
    unsigned int	lb_bm_sconn;	// bitmap of Connected Sender proxies   
    unsigned int	lb_bm_rconn;	// bitmap of Connected Receiver proxies 
	
	int				lb_nr_active;	// number of Connected server nodes reacheable by ping) 
	int				lb_nr_proxy;	// number of Connected proxies 
	int				lb_nr_init;		// number of   initialized  server nodes 

// END MULTIPLE ACCESS FIELDS 
	
	int				lb_nr_cltpxy;	// # of defined Client Proxies (from configuration file)
	int				lb_nr_svrpxy;    // # of defined Server Proxies (from configuration file)

	int				lb_nr_proxies;   //  # of defined  PROXIES (lb_nr_cltpxy + lb_nr_svrpxy -1)
	int				lb_nr_services;	// # of defined Services (from configuration file)
    unsigned int	lb_bm_svrpxy;	// bitmap of defined Server Proxies

#ifdef SPREAD_MONITOR 
    pthread_t 		lb_thread;
    mailbox			lb_mbox;
	int				lb_len;
    char 			lb_sp_group[MAXNODENAME];
    char    		lb_mbr_name[MAX_MEMBER_NAME];
    char    		lb_priv_group[MAX_GROUP_NAME];
    membership_info lb_memb_info;
    vs_set_info     lb_vssets[MAX_VSSETS];
    unsigned int    lb_my_vsset_index;
    int             lb_num_vs_sets;
    char            lb_members[MAX_MEMBERS][MAX_GROUP_NAME];
    char		   	lb_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
    int		   		lb_sp_nr_mbrs;
    char		 	lb_mess_in[MAX_MESSLEN];

#else // SPREAD_MONITOR 
    unsigned int	lb_bm_suspect;	// bitmap of first chance suspected  nodes
	unsigned int 	lb_bm_echo;		// bitmap of nodes to test in each cycle 
	
    pthread_t 		lb_fd_thread;

#endif // SPREAD_MONITOR 

	pthread_mutex_t lb_mtx;  		// to protect the this structure

} lb_t;

#define LB1_FORMAT 	"lb_name=%s lb_nodeid=%d lb_lowwater=%d lb_highwater=%d lb_period=%d lb_hellotime=%d\n"
#define LB1_FIELDS(p)  p->lb_name, p->lb_nodeid, p->lb_lowwater, p->lb_highwater, p->lb_period, p->lb_hellotime
#define LB2_FORMAT 	"lb_name=%s lb_nodeid=%d lb_nr_nodes=%d lb_nr_init=%d lb_bm_nodes=%0lX lb_bm_init=%0lX\n"
#define LB2_FIELDS(p)  p->lb_name, p->lb_nodeid, p->lb_nr_nodes, p->lb_nr_init, p->lb_bm_nodes, p->lb_bm_init
#define LB3_FORMAT 	"lb_name=%s lb_cltname=%s lb_svrname=%s lb_cltdev=%s lb_svrdev=%s\n"
#define LB3_FIELDS(p)  p->lb_name, p->lb_cltname, p->lb_svrname, p->lb_cltdev, p->lb_svrdev 
#define LB4_FORMAT 	"lb_name=%s lb_ssh_host=%s lb_ssh_user=%s lb_ssh_pass=%s\n"
#define LB4_FIELDS(p)  p->lb_name, p->lb_ssh_host, p->lb_ssh_user, p->lb_ssh_pass
#define LB5_FORMAT 	"lb_name=%s lb_start=%d lb_stop=%d lb_min_servers=%d lb_max_servers=%d\n"
#define LB5_FIELDS(p)  p->lb_name, p->lb_start, p->lb_stop, p->lb_min_servers, p->lb_max_servers
#define LB6_FORMAT 	"lb_name=%s lb_vm_start=%s lb_vm_stop=%s lb_vm_status=%s\n"
#define LB6_FIELDS(p)  p->lb_name, p->lb_vm_start, p->lb_vm_stop, p->lb_vm_status  
#define LB7_FORMAT 	"lb_name=%s lb_nr_cltpxy=%d lb_nr_svrpxy=%d lb_nr_proxies=%d lb_nr_services=%d lb_bm_svrpxy=%08X\n"
#define LB7_FIELDS(p)  p->lb_name, p->lb_nr_cltpxy, p->lb_nr_svrpxy,p->lb_nr_proxies, p->lb_nr_services, p->lb_bm_svrpxy

struct thread_desc_s {
    pthread_t 		td_thread;
	proxy_hdr_t 	*td_header;
	proxy_hdr_t 	*td_header2;
	proxy_hdr_t 	*td_header3;
	proxy_hdr_t 	*td_pseudo;
	proxy_payload_t *td_payload;	/* uncompressed payload 		*/	
	proxy_payload_t *td_payload2;	/* uncompressed payload 		*/	
	char 			*td_comp_pl;	/* compressed payload 		*/ 	
	char 			*td_decomp_pl;	/* decompressed payload 		*/ 	
	proxy_payload_t *td_batch;
	int				td_batch_nr;		// number of batching commands
	int				td_cmd_flag;		// signals a command to be sent
	int 			td_msg_ok;
	int 			td_msg_fail;	
	pthread_mutex_t td_mtx;    /* mutex & condition to allow main thread to
								wait for the new thread to  set its TID */
	pthread_cond_t  td_cond;   /* '' */
	pthread_cond_t  td_tcond;   /* '' */
	pthread_cond_t  td_pws_cond;
	pid_t           td_tid;     /* to hold new thread's TID */

	pthread_mutex_t td_mutex;    

	LZ4F_errorCode_t td_lz4err;
	size_t			td_offset;
	size_t			td_maxCsize;		/* Maximum Compressed size */
	size_t			td_maxRsize;		/* Maximum Raw size		 */
	__attribute__((packed, aligned(4)))
	LZ4F_compressionContext_t 	td_lz4Cctx __attribute__((aligned(8))); /* Compression context */
	LZ4F_decompressionContext_t td_lz4Dctx __attribute__((aligned(8))); /* Decompression context */
};
typedef struct thread_desc_s thread_desc_t;

struct  px_stats_s {
	int					pst_snode;
	int					pst_dnode;
	int					pst_dcid;
//	int					pst_src_ep;
//	int					pst_dst_ep;
	unsigned long int 	pst_nr_msg;
	unsigned long int 	pst_nr_data;
	unsigned long int 	pst_nr_cmd;
};
typedef struct px_stats_s px_stats_t;

typedef struct {
	char *px_name;				// proxy name from configuration file 
	int	px_proxyid;				// proxy proxyid
	int px_proto;				// proxy protocol 
	int px_rport;				// proxy receiver port (TCP , UDP)  
	int px_sport;				// proxy sender  port (TCP , UDP)  
	int px_batch;				// batch flag 
	int px_compress;			// compress flag
	int px_autobind;			// autobind flag
	char *px_rname;				// string pointer to receiver name address or "ANY" 
	thread_desc_t px_rdesc;
	thread_desc_t px_sdesc;
  	unsigned long px_snd_seq;		/* send sequence #  - filled and controled by proxies not by M3-IPC 			*/
  	unsigned long px_ack_seq;		/* acknowledge sequence #  - filled and controled by proxies not by M3-IPC 	*/
	pthread_mutex_t px_send_mtx;
	
	pthread_mutex_t px_conn_mtx;  
	pthread_cond_t  px_conn_scond;
	pthread_cond_t  px_conn_rcond;
	
	struct sockaddr_in px_rmtclient_addr;
	struct sockaddr_in px_rmtserver_addr;
	int    px_rlisten_sd;
	int    px_rconn_sd;
	int    px_sproxy_sd;
	unsigned int px_status;
	struct hostent *px_rmthost;
	pthread_t px_pws_pth;
	int 	px_pws_port;
	px_stats_t px_stat[NR_DCS];
	LZ4F_errorCode_t p_lz4err;
	size_t			px_offset;
	size_t			px_maxCsize;		/* Maximum Compressed size */
	size_t			px_maxRsize;		/* Maximum Raw size		 */
	LZ4F_compressionContext_t 	px_lz4Cctx __attribute__((aligned(8))); /* Compression context */
	LZ4F_decompressionContext_t px_lz4Dctx __attribute__((aligned(8))); /* Decompression context */

}proxy_t;
#define PROXY_FORMAT 	   "px_name=%s px_proxyid=%d px_proto=%d px_rport=%d px_sport=%d px_batch=%d px_compress=%d px_autobind=%d px_rname=%s\n"
#define PROXY_FIELDS(p)  	p->px_name, p->px_proxyid, p->px_proto, p->px_rport, p->px_sport, p->px_batch, p->px_compress, p->px_autobind, p->px_rname

#include "dsp_glo.h"
#include "../debug.h"
#include "../macros.h"

void lb_config(char *f_conf);	/* config file name. */
void init_spread(void);
void *svr_Rproxy(void *arg);
void *svr_Sproxy(void *arg);
void *clt_Rproxy(void *arg);
void *clt_Sproxy(void *arg);
void *lb_monitor(void *arg);
void *lb_fd(void *arg);

int clt_Rproxy_svrmq(client_t *clt_ptr, server_t *svr_ptr,	sess_entry_t *sess_ptr);
int  ucast_cmd(int agent_id, char *agent_name, char *cmd);
void check_server_idle(server_t *svr_ptr);
void start_new_node(char *rmt_cmd);
int check_node_status(server_t *svr_ptr);
int stop_idle_node(server_t *svr_ptr);
void init_session( sess_entry_t *sess_ptr);
void init_service(service_t *svc_ptr);
void init_client(client_t *clt_ptr);
void init_server(server_t *svr_ptr);
void init_lb(void );
int send_hello_msg(proxy_t *px_ptr);
int send_load_threadholds(server_t *svr_ptr);
int send_rmtbind(server_t *svr_ptr, int dcid, int endpoint, int nodeid, char *pname);
void *lb_fd_monitor(void *arg);
int enable_keepalive(int sock);



