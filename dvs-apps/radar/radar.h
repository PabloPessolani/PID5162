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

#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <net/if.h>
#include <net/if_arp.h>   
#include <arpa/inet.h>

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

#define BUFF_SIZE		MAXCOPYBUF
#define MAX_MESSLEN     (BUFF_SIZE+1024)
#define MAX_VSSETS      10
#define MAX_MEMBERS     NR_NODES
#define NR_DEVS         2		/* number of minor devices - example*/

#include <sp.h>

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
	
#define NO_PRIMARY_BIND		(-1)
#define NO_PRIMARY_DEAD		(-2)
#define NO_PRIMARY_NET 		(-3)

#define RADAR_TIMEOUT_SEC	5
#define RADAR_TIMEOUT_MSEC	0
#define RADAR_ERROR_SPEEP	5

#define MC_RADAR_INFO 		0xDA

#define	NR_MAX_CONTROL		32

#define REPLICA_RPB		0
#define REPLICA_RSM 	1

#define REPL_ANY_NODES		(-1) 
#define MAX_RANDOM_RETRIES	sizeof(unsigned int) 

typedef struct {
	int			    rad_index;
	int 			rad_dcid;
	int				rad_ep;
	int				rad_replication;		// PB or FSM 
	
	char 			rad_svrname[MAXPROCNAME];
	int				rad_len;
	int				rad_primary_mbr;
	int				rad_primary_old;
	int				rad_nr_nodes;
	int 			rad_nr_init;
	int				rad_nr_radar;
	char 			rad_group[MAXPROCNAME];
	
	unsigned int	rad_bm_nodes;		// Connected nodes 
	unsigned int	rad_bm_init;		// nodes which can be primary (PB) or active nodes (FSM)
	unsigned int	rad_bm_radar;		// nodes where radar is
	unsigned int	rad_bm_valid;		// valid replication nodes 
	
	pthread_t 		rad_thread;

	mailbox			rad_mbox;
	char 			rad_sp_group[MAXNODENAME];
	char    		rad_mbr_name[80];
	
	char    		rad_priv_group[MAX_GROUP_NAME];
	membership_info rad_memb_info;
	vs_set_info     rad_vssets[MAX_VSSETS];
	unsigned int    rad_my_vsset_index;
	int             rad_num_vs_sets;
	char            rad_members[MAX_MEMBERS][MAX_GROUP_NAME];
	char		   	rad_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
	int		   		rad_sp_nr_mbrs;
	
	char		 	rad_mess_in[MAX_MESSLEN];

} radar_t;

#define RAD1_FORMAT 	"rad_dcid=%d rad_ep=%d rad_len=%d rad_svrname=%s rad_mbr_name=%s\n"
#define RAD1_FIELDS(p) p->rad_dcid, p->rad_ep, p->rad_len, p->rad_svrname, p->rad_mbr_name

#define RAD2_FORMAT "rad_primary_mbr=%d rad_primary_old=%d rad_nr_nodes=%d rad_nr_init=%d \n"
#define RAD2_FIELDS(p) p->rad_primary_mbr, p->rad_primary_old, p->rad_nr_nodes, p->rad_nr_init
	
#define RAD3_FORMAT "rad_bm_nodes=%X rad_bm_init=%X\n"
#define RAD3_FIELDS(p) p->rad_bm_nodes, p->rad_bm_init

#include "glo.h"
#include "../debug.h"
#include "../macros.h"



