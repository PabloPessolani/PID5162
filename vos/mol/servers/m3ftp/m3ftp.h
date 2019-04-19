
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

#include "../../../../include/com/dvs_config.h"
#include "../../include/sys_config.h"
#include "../../../../include/com/config.h"
#include "../../../../include/com/const.h"
#include "../../include/const.h"
#include "../../../../include/com/types.h"
#include "../../include/types.h"
#include "../../include/type.h"
#include "../../include/limits.h"
#include "../../../../include/com/timers.h"
#include "../../include/timers.h"

#include "../../../../include/com/dvs_usr.h"
#include "../../../../include/com/dc_usr.h"
#include "../../../../include/com/node_usr.h"
#include "../../../../include/com/proc_usr.h"
#include "../../../../include/com/proc_sts.h"
#include "../../../../include/com/com.h"
#include "../../include/com.h"
#include "../../../../include/com/ipc.h"
#include "../../include/kipc.h"
#include "../../include/mollib.h"
#include "../../../../include/com/cmd.h"
#include "../../../../include/com/proxy_usr.h"
#include "../../../../include/com/proxy_sts.h"
#include "../../../../include/com/dvs_errno.h"
#include "../../../../include/com/endpoint.h"
#include "../../include/resource.h"
#include "../../include/callnr.h"
#include "../../include/ansi.h"
#include "../../../../include/com/priv_usr.h"
#include "../../../../include/com/priv.h"
#include "../../include/signal.h"
#include "../../include/slots.h"
#include "../../include/syslib.h"
#include "../../../../include/com/stub_dvkcall.h"

#include "../debug.h"
#include "../macros.h"

extern int	dvk_fd;
extern int local_nodeid;

// REQUESTS
#define FTP_NONE	0
#define FTP_GET		1
#define FTP_PUT		2
#define FTP_NEXT	3
#define FTP_CANCEL  4

#define FTPOPER 	m_type	// request type 
#define FTPPATH 	m1_p1 	// pathname address
#define FTPPLEN  	m1_i1 	// lenght of pathname 
#define FTPDATA 	m1_p2 	// DATA  address
#define FTPDLEN  	m1_i2 	// lenght of DATA 




