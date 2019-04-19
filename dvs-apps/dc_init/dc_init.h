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
#include <limits.h>
#include <fcntl.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/sysinfo.h> 
#include <sys/stat.h>
#include <sys/syscall.h> 
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/mount.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define DVS_USERSPACE	1
#define _GNU_SOURCE
#define  __USE_GNU
#include <sched.h>
#define cpumask_t cpu_set_t

#include <sp.h>

#include "../../include/com/dvs_config.h"
#include "../../include/com/config.h"
#include "../../include/com/com.h"
#include "../../include/com/const.h"
#include "../../include/com/cmd.h"
#include "../../include/com/priv_usr.h"
//#include "../../include/com/priv.h"
#include "../../include/com/dc_usr.h"
#include "../../include/com/node_usr.h"
#include "../../include/com/proc_sts.h"
#include "../../include/com/proc_usr.h"
//#include "../../include/com/proc.h"
#include "../../include/com/proxy_sts.h"
#include "../../include/com/proxy_usr.h"
#include "../../include/com/dvs_usr.h"
#include "../../include/com/dvk_calls.h"
#include "../../include/com/dvk_ioctl.h"
#include "../../include/com/dvs_errno.h"
#include "../../include/dvk/dvk_ioparm.h"
#include "../../include/com/stub_dvkcall.h"

#include "../../include/generic/tracker.h"

#include "../macros.h"
#include "../debug.h"

extern int	dvk_fd;

#define IDLEN 16
#define IPLEN 16
#define IMAGELEN 127

typedef struct container {
	dc_usr_t c_dcu; 
    int  c_pipe_fd[2];  			/* Pipe used to synchronize parent and child */
    char **c_args; 					/* Container command and arguments */
    char c_ip_addr[IPLEN + 1]; 		/* IP address of the container */
    char c_image[IMAGELEN + 1]; 	/* Path of the container image $(PWD)/images/<image> */
    char c_mount[IMAGELEN + 1]; 	/* Path of the mount point of the root directory  */
	long c_memory;
} container_t;







