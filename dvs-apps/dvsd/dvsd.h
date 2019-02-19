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
#define  __USE_GNU
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

#include "sp.h"
#include "../debug.h"
#include "../macros.h"

#include "config.h"
#include "dvscmd.h"
//#include "table.h"
#include "proto.h"






