
#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <sched.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/sysinfo.h> 
#include <sys/stat.h>
#include <sys/syscall.h> 
#include <sys/mman.h>
#include <fcntl.h>
#include <malloc.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define DVS_USERSPACE	1
#define _GNU_SOURCE
#include <sched.h>
#define cpumask_t cpu_set_t
#define USRDBG 1

#include "/usr/src/dvs/include/com/com.h"

#include "/usr/src/dvs/include/com/dvs_config.h"

#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/const.h"

#include "/usr/src/dvs/include/com/kipc.h"

#include "/usr/src/dvs/include/com/types.h"
#include "/usr/src/dvs/include/com/timers.h"

#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/com/endpoint.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
#include "/usr/src/dvs/include/com/priv.h"
#include "/usr/src/dvs/include/com/stub_dvkcall.h"

#include "../debug.h"
#include "../macros.h"

extern int	dvk_fd;
extern int local_nodeid;

#define MNX_PATH_MAX    1024
#define MAXBUFLEN		MAXCOPYLEN // MAXCOPYBUF
#define	MAX_RETRIES		3
#define SEND_RECV_MS 	30000
#define	BIND_TIMEOUT	1000


// REQUESTS
#define FTP_NONE	0
#define FTP_GET		1
#define FTP_PUT		2
#define FTP_GNEXT	3
#define FTP_PNEXT	4
#define FTP_CANCEL  5

#define FTPOPER 	m_type	// request type 
#define FTPPATH 	m1_p1 	// pathname address
#define FTPPLEN  	m1_i1 	// lenght of pathname 
#define FTPDATA 	m1_p2 	// DATA  address
#define FTPDLEN  	m1_i2 	// lenght of DATA 
#define FTPPOS  	m1_i3 	// DATA position 


