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

#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <net/if_arp.h>   
#include <arpa/inet.h>

#include <lz4frame.h>

#define DVS_USERSPACE	1
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
#include "../include/generic/hello.h"

typedef unsigned long long jiff;

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

typedef struct {
		int				mpa_nodeid;
		int				mpa_pid;
		int 			mpa_nr_proxies;
		int 			mpa_param_bm;
		int 			mpa_compress_opt;

		int				mpa_lowwater; 		// low water load (0-100)
		int				mpa_highwater;		// low water load (0-100)
		int				mpa_period;			// load measurement period in seconds (1-3600)

		int				mpa_load_lvl;
		int				mpa_cpu_usage;
	
	    unsigned int	mpa_bm_nodes;		// bitmap of Connected nodes
		unsigned int	mpa_bm_init;		// bitmap  initialized/active nodes 
		int				mpa_nr_nodes;
		int				mpa_nr_init;
		
		pthread_mutex_t mpa_mutex;    	

} mpa_t;
#define MPA0_FORMAT 	"mpa_nodeid=%d mpa_pid=%d mpa_nr_proxies=%d mpa_param_bm=%08X mpa_compress_opt=%d\n"
#define MPA0_FIELDS(p)  p->mpa_nodeid, p->mpa_pid, p->mpa_nr_proxies, p->mpa_param_bm, p->mpa_compress_opt
#define MPA1_FORMAT 	"mpa_lowwater=%d mpa_highwater=%d mpa_period=%d\n"
#define MPA1_FIELDS(p)  p->mpa_lowwater, p->mpa_highwater, p->mpa_period
#define MPA2_FORMAT 	"mpa_nr_nodes=%d mpa_nr_init=%d mpa_bm_nodes=%0X mpa_bm_init=%08X\n"
#define MPA2_FIELDS(p)  p->mpa_nr_nodes, p->mpa_nr_init, p->mpa_bm_nodes, p->mpa_bm_init
#define MPA3_FORMAT 	"mpa_load_lvl=%d mpa_cpu_usage=%d\n"
#define MPA3_FIELDS(p)  p->mpa_load_lvl, p->mpa_cpu_usage

#include "multi_glo.h"
#include "debug.h"
#include "macros.h"

void multi_config(char *f_conf);	/* config file name. */





