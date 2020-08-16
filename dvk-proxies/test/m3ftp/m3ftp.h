#include <asm/ptrace.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/syscall.h>    /* For SYS_xxx definitions */
#include <linux/limits.h>

#include <fcntl.h>
#define DVS_USERSPACE	1
#define _GNU_SOURCE
#define  __USE_GNU
#include <sched.h>
#define cpumask_t cpu_set_t

#include "../../../include/com/dvs_config.h"
#include "../../../include/com/config.h"
#include "../../../include/com/com.h"
#include "../../../include/com/const.h"
#include "../../../include/com/cmd.h"
#include "../../../include/com/priv_usr.h"
//#include "../../../include/com/priv.h"
#include "../../../include/com/dc_usr.h"
#include "../../../include/com/node_usr.h"
#include "../../../include/com/proc_sts.h"
#include "../../../include/com/proc_usr.h"
//#include "../../../include/com/proc.h"
#include "../../../include/com/proxy_sts.h"
#include "../../../include/com/proxy_usr.h"
#include "../../../include/com/dvs_usr.h"
#include "../../../include/com/dvk_calls.h"
#include "../../../include/com/dvk_ioctl.h"
#include "../../../include/com/dvs_errno.h"
#include "../../../include/dvk/dvk_ioparm.h"
#include "../../../include/com/stub_dvkcall.h"


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




