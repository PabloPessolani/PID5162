#define _MULTI_THREADED
#define _GNU_SOURCE     
#define  MOL_USERSPACE	1

#define MUKDBG    1

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
#include "../../include/config.h"
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
#include "../../include/signal.h"
#include "../../include/slots.h"
#include "../../include/syslib.h"
#include "../../include/mollib.h"
#include "../../../../include/com/stub_dvkcall.h"

#include "../../debug.h"
#include "../../macros.h"

extern int	dvk_fd;

#include "const.h"
#include "../../include/lock.h"
#include "../../include/dmap.h"
#include "../../include/dir.h"


#include "../../debug.h"
#include "../../macros.h"
#include "../pm/mproc.h"

#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */


#ifndef UTILITY_C
#include "../../include/statfs.h"
#include "../../include/mnx_stat.h"
#endif // UTILITY_C

#include "../../include/unistd.h"
#include "../../include/fcntl.h"
#include "../../include/ioctl.h"
#include "../../include/svrctl.h"
#include "../../include/ioc_tty.h"
#include "../../include/select.h"

//Para lectura de configuracion de MOLFS
#include "../../../../include/generic/configfile.h"

#include "type.h"
#include "fproc.h"
#include "param.h"
#include "glo.h"
#include "inode.h"
#include "proto.h"
#include "super.h"
#include "buf.h"
#include "file.h"
// #include "select.h" //ver sys_setalarm y demas

#define     MAJOR2TAB(major)	fs_dmap_tab[fs_dmap_rev[major]]
#define     DEV2TAB(dev)		fs_dmap_tab[fs_dmap_rev[(dev >> MNX_MAJOR) & BYTE]]
#define 	MM2DEV(major, minor)	((major << MNX_MAJOR) | minor)
#define 	DEV2MAJOR(dev)			((dev >> MNX_MAJOR) & BYTE)
#define 	DEV2MINOR(dev)			((dev >> MNX_MINOR) & BYTE)

extern char *program_invocation_name;
extern char *program_invocation_short_name;
