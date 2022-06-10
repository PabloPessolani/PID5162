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
#define	MAX_MEMBER_NAME		64

#define MAX_CMDIN_LEN		128
#define MAX_LINE_LEN		128
#define MAX_CMDOUT_LEN		(64*1024)

#define DD_TIMEOUT_5SEC	5
#define DD_TIMEOUT_MSEC	0
#define DD_ERROR_SPEEP	5

#define DD_INVALID		(-1) 
#define MAX_PIPE_NAME 	1024

#define DD_NOTINIT		(-1) 

#define DD_DELIMITER 	","

#include <sp.h>

#define SPREAD_PORT "4803"
//nombre del grupo de spread
#define SPREAD_GROUP "DDGROUP"
#define SHARP_GROUP  "#DDGROUP"
//nombre con el cual se va a identificar al balancer
#define DDM_NAME  "DDM"
#define DDM_SHARP "#DDM"

//Cantidad de nodos que se almacenan en el struct (podria ser diferente en ambos?)
#define MAX_AGENT_NODES NR_NODES
#define DDA_NAME 	"DDA"
#define DDA_SHARP  	"#DDA"

#define SOURCE_MONITOR		1
#define SOURCE_PROXY		2
#define SOURCE_AGENT 		3

#define MAX_MSG_STRING		128
#define MT_ACKNOWLEDGE 		1000
#define MT_HELLO_AGENTS		100		// From MONITOR to AGENTS
#define MT_HELLO_MONITOR	(MT_HELLO_AGENTS + MT_ACKNOWLEDGE) 
#define MT_CMD2AGENT		101		// From MONITOR to AGENTS
#define MT_OUT2MONITOR		(MT_CMD2AGENT + MT_ACKNOWLEDGE)


#define MONITOR_OUTCMD_FIFO 	"/tmp/OUTCMD_%s"
#define MONITOR_INCMD_FIFO  	"/tmp/INCMD_%s"

#define USRDBG 1

#include "../debug.h"
#include "../macros.h"
#define TRUE 1



