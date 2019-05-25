#define _MULTI_THREADED
#define _XOPEN_SOURCE 600
#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _DEFAULT_SOURCE 
#define _POSIX_C_SOURCE 

#include <asm/ptrace.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <netdb.h>
#include <assert.h>
 
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/sysinfo.h> 
#include <sys/sem.h>
#include <sys/stat.h> 
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/ioctl.h>	   
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
		      
#include <net/if.h>
#include <net/if_arp.h>
		   
//TIPC include
#include <linux/tipc.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if_tun.h>
//#include <netinet/ether.h>
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <linux/if_ether.h>   // ETH_P_ARP = 0x0806
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO

#include <getopt.h>

#define DVS_USERSPACE
//#define _GNU_SOURCE
//#define  __USE_GNU
//#include <sched.h>
//#define cpumask_t cpu_set_t

#include "../include/com/dvs_config.h"
#include "../include/com/config.h"
#include "../include/com/com.h"
#include "../include/com/const.h"
#include "../include/com/cmd.h"
#include "../include/com/types.h"
#include "../include/com/timers.h"
#include "../include/com/ipc.h"
#include "../include/com/endpoint.h"
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
//#include "../include/com/proxy.h"
#include "../include/com/node_usr.h"

//#include "../kernel/minix/kipc.h"
//#include "../kernel/minix/ansi.h"
//#include "../kernel/minix/resource.h"
//#include "../kernel/minix/signal.h"
//#include "../kernel/minix/net/gen/ether.h"
//#include "../kernel/minix/net/gen/eth_io.h"
//#include "../kernel/minix/net/gen/in.h"

#ifndef __cplusplus
#include "../include/com/stub_dvkcall.h"
#else
extern "C" {
#include "./stub4cpp.h"	
}
#endif

#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */

extern int	dvk_fd;
extern int h_errno;


