#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <ctype.h>
#include <netdb.h>
#include <assert.h>

extern int h_errno;

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/sysinfo.h> 
#include <sys/sem.h>
#include <sys/stat.h> 
#include <sys/ipc.h>
//#include <limits.h>
#include <sys/msg.h>
#include <sys/mman.h>

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
#include "../../include/slots.h"
#include "../../../../include/com/stub_dvkcall.h"

#include "sp.h"

#include "../debug.h"
#include "../macros.h"

#include "config.h"
#include "system.h"
#include "table.h"
#include "proto.h"
#include "clock.h"
#include "glo.h"

#define PROC_MAPPED(p_nr) ((( p_nr + dc_ptr->dc_nr_tasks ) * dvs_ptr->d_size_proc) + kproc)

extern int	dvk_fd;
extern int local_nodeid;



