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
#include "../include/com/dvk_calls.h"
#include "../include/com/dvk_ioctl.h"
#include "../include/com/dvs_errno.h"
#include "../include/dvk/dvk_ioparm.h"
#include "../include/com/stub_dvkcall.h"

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
#define MAX_RANDOM_RETRIES	sizeof(unsigned int)
#define MAXCMDLEN 		1024

#define LVL_NOTINIT		(-1) 
#define	LVL_UNLOADED	0
#define	LVL_LOADED		1
#define	LVL_SATURATED	2

#define	NO		0
#define	YES		1

#define	MAX_SVC_NR	(sizeof(unsigned long int) * 8)
#define LBP_BASE_PORT     		3000
#define	LBP_CLT2SVR				1111
#define	LBP_SVR2CLT				2222
#define	LBP_SVR2SVR				3333
#define	LBP_CLT2CLT				4444
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
	int	svc_dcid;				// server dcid
	int svc_extep;				// server external endpoint 
	int svc_minep;				// server lower endpoint when execute the server 
	int svc_maxep;				// server higher endpoint when execute the server 
	int svc_bind;				// Which kind of bind must be done on server creation
	char *svc_prog;				// Program to execute on server
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
	int	clt_nodeid;
	int	clt_lbRport;			// LB Receiver port 
	int clt_cltRport;			// Client Receiver port 
	int	clt_compress;			// Enable LZ4 Compression 0:NO 1:YES
	int	clt_batch;				// Enable message batching 0:NO 1:YES
	
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

typedef struct {
	char *svr_name;			// server name from configuration file 
	int	svr_nodeid;			// server nodeid
	int svr_lbRport;		// LB Receiver port 
	int svr_svrRport;		// Server Receiver port 
	int	svr_level;			// Load LEVEL 		
    int	svr_load;			// CPU Load (0-100) . Value (-1) implies INVALID
	int	svr_compress;		// Enable LZ4 Compression 0:NO 1:YES
	int	svr_batch;			// Enable message batching 0:NO 1:YES

	char *svr_start;		// string to command which START the server NODE 
	char *svr_stop;			// string to command which STOP the server NODE 
	char *svr_image;		// string to command which START the server NODE 
	
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

	pthread_mutex_t svr_agent_mtx;  // controls when a server process is started or killed 
	pthread_cond_t  svr_agent_cond;  

	pthread_mutex_t svr_node_mtx;  // controls when a complete server node VM is started or stopped 
	pthread_cond_t  svr_node_cond;  
	
	TAILQ_HEAD(svr_tailhead,client_s) svr_tail_head;
	pthread_mutex_t svr_tail_mtx;  		// to protect the linked list
	
}server_t;
#define SERVER_FORMAT 	"svr_name=%s svr_nodeid=%d svr_lbRport=%d svr_svrRport=%d svr_level=%d svr_load=%d svr_bm_svc=%lX\n"
#define SERVER_FIELDS(p) p->svr_name, p->svr_nodeid, p->svr_lbRport, p->svr_svrRport, p->svr_level, p->svr_load,  p->svr_bm_svc 
#define SERVER1_FORMAT 	"svr_name=%s svr_nodeid=%d svr_compress=%d svr_batch=%d\n"
#define SERVER1_FIELDS(p) p->svr_name, p->svr_nodeid, p->svr_compress, p->svr_batch 



#define	MAX_MEMBER_NAME		64

typedef struct {
	char 	*lb_name;
	int		lb_nodeid;
	int		lb_lowwater; 		// low water load (0-100)
	int		lb_highwater;		// low water load (0-100)
	int		lb_period;			// load measurement period in seconds (1-3600)

	char 	*lb_cltname;		// hostname of this LB inside CLIENT NETWORK
	char 	*lb_svrname;		// hostname of this LB inside SERVER NETWORK
	char 	*lb_cltdev;			// device name of this LB inside CLIENT NETWORK i.e eth1
	char 	*lb_svrdev;			// device name of this LB inside SERVER NETWORK  i.e eth0

    pthread_t 		lb_thread;

    unsigned int	lb_bm_lbparms;	// bitmap of Config Load Balancer Parameters
    unsigned int	lb_bm_svcparms;	// bitmap of Config Service Parameters
    unsigned int	lb_bm_cltparms;	// bitmap of Config Client Parameters
    unsigned int	lb_bm_svrparms;	// bitmap of Config Server Parameters

    unsigned int	lb_bm_nodes;	// bitmap of Connected nodes
    unsigned int	lb_bm_init;		// bitmap  initialized/active nodes 
	
	int				lb_nr_nodes;
	int				lb_nr_init;
	
	int				lb_nr_cltpxy;	// # of Client Proxies (from configuration file)
	int				lb_nr_svrpxy;    // # of Server Proxies (from configuration file)
	int				lb_nr_services;	// # of Services (from configuration file)

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
	
} lb_t;

#define LB1_FORMAT 	"lb_name=%s lb_nodeid=%d lb_lowwater=%d lb_highwater=%d lb_period=%d\n"
#define LB1_FIELDS(p)  p->lb_name, p->lb_nodeid, p->lb_lowwater, p->lb_highwater, p->lb_period
#define LB2_FORMAT 	"lb_name=%s lb_nodeid=%d lb_nr_nodes=%d lb_nr_init=%d lb_bm_nodes=%0lX lb_bm_init=%0lX\n"
#define LB2_FIELDS(p)  p->lb_name, p->lb_nodeid, p->lb_nr_nodes, p->lb_nr_init, p->lb_bm_nodes, p->lb_bm_init
#define LB3_FORMAT 	"lb_name=%s lb_cltname=%s lb_svrname=%s lb_cltdev=%s lb_svrdev=%s\n"
#define LB3_FIELDS(p)  p->lb_name, p->lb_cltname, p->lb_svrname, p->lb_cltdev, p->lb_svrdev 

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

int clt_Rproxy_svrmq(client_t *clt_ptr, server_t *svr_ptr,	sess_entry_t *sess_ptr);
int  ucast_cmd(int agent_id, char *agent_name, char *cmd);




