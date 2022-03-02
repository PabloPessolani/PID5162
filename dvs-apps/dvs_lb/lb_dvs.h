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
#include <time.h>

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
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <net/if_arp.h>   
#include <arpa/inet.h>

#include "lb_spread.h"

#define DVS_USERSPACE	1
#define USRDBG 			1	

#define _GNU_SOURCE
//#define  __USE_GNU
#include <sched.h>
#define cpumask_t cpu_set_t

#include "../include/com/dvs_config.h"
#include "../include/com/config.h"
#include "../include/com/com.h"
#include "../include/com/const.h"
#include "../include/com/cmd.h"
#include "../include/com/priv_usr.h"
//#include "../include/com/priv.h"
#include "../include/com/dc_usr.h"
#include "../include/com/node_usr.h"
#include "../include/com/proc_sts.h"
#include "../include/com/proc_usr.h"
//#include "../include/com/proc.h"
#include "../include/com/proxy_sts.h"
#include "../include/com/proxy_usr.h"
#include "../include/com/dvs_usr.h"
#include "../include/com/ipc.h"
#include "../include/com/dvk_calls.h"
#include "../include/com/dvk_ioctl.h"
#include "../include/com/dvs_errno.h"
#include "../include/dvk/dvk_ioparm.h"
#include "../include/com/stub_dvkcall.h"
#include "../include/generic/hello.h"

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

#define PING_PACKET_SIZE     4096
#define PING_WAIT_TIME   5
#define PING_NR_PACKETS  3

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

#include <sp.h>
#include <lz4frame.h>

#define BLOCK_16K	(16 * 1024)


#define SPREAD_PORT "4803"
#define c_pl_ptr c_snd_seq 

#define FREE(x) do{ \
		USRDEBUG("FREE %p\n",x);\
		free(x);\
		}while(0)

typedef unsigned long long jiff;

/*Spread message: message m3-ipc, transfer data*/
// typedef struct {message msg; unsigned buffer_data[BUFF_SIZE];} SP_message;	 
typedef struct {
    message msg; 
    struct{
        int flag_buff;	/*compress or uncompress data into buffer*/
        long buffer_size; /* bytes compress or uncompress */
        unsigned buffer_data[BUFF_SIZE];
    } buf;
}SP_message;	

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

typedef struct  {
	proxy_hdr_t 	pm_header;
	proxy_payload_t pm_payload;
} proxy_msg_t;
		
typedef struct {
	long 			mb_type;
	proxy_hdr_t 	mb_header;
} msgq_buf_t;
	
typedef struct {
	int				st_nr_sess; 	// # of active sessions
	pthread_mutex_t st_mutex; 		// per Table Mutex
	sess_entry_t 	*st_tab_ptr;	// pointer to the first table element 
} sess_tab_t;

	
// Load Balancer Proxy Descriptor
struct lbpx_desc_s {
    pthread_t 		lbp_thread;
	proxy_hdr_t 	*lbp_header;
	proxy_payload_t *lbp_payload;	/* uncompressed payload 		*/	
	
	msgq_buf_t		*lbp_mqbuf;		// Server proxy sender message queue 
	int				lbp_mqid;
	int				lbp_mqkey;
	struct msqid_ds lbp_mqds;
	
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
#define LBPX_MSGQ_FORMAT 	"lbp_mqid=%d lbp_mqkey=%d lbp_mqds.msg_qbytes=%d\n"
#define LBPX_MSGQ_FIELDS(p)  p->lbp_mqid, p->lbp_mqkey, p->lbp_mqds.msg_qbytes

#define CMD_TSFORMAT 		"c_timestamp=%lld.%.9ld\n" 
#define CMD_TSFIELDS(p) 	(long long) p->c_timestamp.tv_sec, p->c_timestamp.tv_nsec
#define CMD_PIDFORMAT 		"c_flags=0x%lX c_pid=%ld\n" 
#define CMD_PIDFIELDS(p) 	p->c_flags, p->c_pid

struct client_s {
	char *clt_name;
	int	clt_index;			// client  array index
	int	clt_nodeid;
	int	clt_lbRport;			// LB Receiver port 
	int clt_cltRport;			// Client Receiver port 
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

#define CLIENT_FORMAT 	"clt_name=%s clt_nodeid=%d clt_lbRport=%d clt_cltRport=%d clt_compress=%d clt_batch=%d\n"
#define CLIENT_FIELDS(p)  p->clt_name, p->clt_nodeid, p->clt_lbRport, p->clt_cltRport,  p->clt_compress, p->clt_batch 

struct server_s{
	char *svr_name;			// server name from configuration file 
	int	svr_index;			// server  array index
	int	svr_nodeid;			// server nodeid
	int svr_lbRport;		// LB Receiver port 
	int svr_svrRport;		// Server Receiver port 
	int	svr_level;			// Load LEVEL 		
    int	svr_load;			// CPU Load (0-100) . Value (-1) implies INVALID
	int	svr_compress;		// Enable LZ4 Compression 0:NO 1:YES
	int	svr_batch;			// Enable message batching 0:NO 1:YES
	
	int svr_icmp_fd;		// used by FD
	int svr_icmp_sent;		// used by FD
	int svr_icmp_rcvd;		// used by FD
	int svr_icmp_seq; 		// used by FD
	int svr_icmp_retry;		// used by FD
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

	pthread_mutex_t svr_node_mtx;  // controls when a complete server node VM is started or stopped 
	pthread_cond_t  svr_node_cond;  
	
    TAILQ_ENTRY(server_s) svr_tail_entry;         /* Tail queue. */

	TAILQ_HEAD(svr_clthead,client_s) svr_clt_head;
	TAILQ_HEAD(svr_svrhead,server_s) svr_svr_head;
	
	pthread_mutex_t svr_tail_mtx;  		// to protect the linked list
	
};
typedef struct server_s server_t;

#define SERVER_FORMAT 	"svr_name=%s svr_nodeid=%d svr_lbRport=%d svr_svrRport=%d svr_level=%d svr_load=%d svr_bm_svc=%lX\n"
#define SERVER_FIELDS(p) p->svr_name, p->svr_nodeid, p->svr_lbRport, p->svr_svrRport, p->svr_level, p->svr_load,  p->svr_bm_svc 
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
    unsigned int	lb_bm_active;	// bitmap of Connected nodes
    unsigned int	lb_bm_init;		// bitmap  initialized nodes 
	int				lb_nr_active;	// number of Connected server nodes 
	int				lb_nr_init;		// number of   initialized  server nodes 

// END MULTIPLE ACCESS FIELDS 
	
	int				lb_nr_cltpxy;	// # of defined Client Proxies (from configuration file)
	int				lb_nr_svrpxy;    // # of defined Server Proxies (from configuration file)
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
    unsigned int	lb_bm_suspect2;	//  bitmap of second chance suspected  nodes	
	unsigned int 	lb_bm_echo;		// bitmap of nodes to test in each cycle 
	
    pthread_t 		lb_fds_thread;
    pthread_t 		lb_fdr_thread;
	pthread_mutex_t lb_fd_mtx;  
	pthread_cond_t  lb_fd_scond;  
	pthread_cond_t  lb_fd_rcond;  

#endif // SPREAD_MONITOR 
	
	msgq_buf_t	   *lb_mqbuf;		// Server proxy sender message queue 
	int				lb_mqid;
	int				lb_mqkey;
	struct msqid_ds lb_mqds;
	
	
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
#define LB7_FORMAT 	"lb_name=%s lb_nr_cltpxy=%d lb_nr_svrpxy=%d lb_nr_services=%d lb_bm_svrpxy=%08X\n"
#define LB7_FIELDS(p)  p->lb_name, p->lb_nr_cltpxy, p->lb_nr_svrpxy, p->lb_nr_services, p->lb_bm_svrpxy

#include "lb_glo.h"
#include "../debug.h"
#include "../macros.h"
#define TRUE 1

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
int send_hello_msg(server_t *svr_ptr);
int send_load_threadholds(server_t *svr_ptr);
int send_rmtbind(server_t *svr_ptr, int dcid, int endpoint, int nodeid, char *pname);
void *lb_fd_sender(void *arg);
void *lb_fd_receiver(void *arg);











