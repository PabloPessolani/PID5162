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
#include <netdb.h>

#include <stdarg.h>
#include <inttypes.h>

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

#include "../../include/com/dvs_config.h"
#include "./include/sys_config.h"
#include "../../include/com/config.h"
#include "../../include/com/const.h"
#include "./include/const.h"
#include "../../include/com/types.h"
#include "./include/types.h"
#include "./include/type.h"
#include "./include/limits.h"
#include "../../include/com/timers.h"
#include "./include/timers.h"

#include "../../include/com/dvs_usr.h"
#include "../../include/com/dc_usr.h"
#include "../../include/com/node_usr.h"
#include "../../include/com/proc_usr.h"
#include "../../include/com/proc_sts.h"
#include "../../include/com/com.h"
#include "./include/com.h"
#include "../../include/com/ipc.h"
#include "./include/kipc.h"
#include "../../include/com/cmd.h"
#include "../../include/com/proxy_usr.h"
#include "../../include/com/proxy_sts.h"
#include "../../include/com/dvs_errno.h"
#include "../../include/com/endpoint.h"
#include "./include/resource.h"
#include "./include/callnr.h"
#include "./include/ansi.h"
#include "../../include/com/priv_usr.h"
#include "../../include/com/priv.h"
#include "./include/slots.h"
#include "./include/syslib.h"
#include "../../include/com/stub_dvkcall.h"
#include "./servers/pm/const.h"
#include "./servers/rs/rs_udp.h"
#include "../../include/generic/configfile.h"

#include "./include/libtask.h"

#include "debug.h"
#include "macros.h"
#include "mutex.h"

#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */
#define MAX_ARG_NR 			20


int  main_systask ( int argc, char *argv[] );
int  main_pm ( int argc, char *argv[] );
int  main_rdisk ( int argc, char *argv[] );
int  main_fs ( int argc, char *argv[] );
int  main_is ( int argc, char *argv[] );
int  main_nweb ( int argc, char *argv[] );
int  main_ftpd ( int argc, char *argv[] );

#define nil ((void*)0)










