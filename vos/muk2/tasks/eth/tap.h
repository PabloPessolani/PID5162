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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <malloc.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_tun.h>
//#include <netinet/ether.h>
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <linux/if_ether.h>   // ETH_P_ARP = 0x0806
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO

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
#include "../../include/u64.h"
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
#include "../../include/partition.h"
#include "../../../../include/com/stub_dvkcall.h"
#include "../../include/syslib.h"
#include "../../include/devio.h"


#include "../../include/net/gen/ether.h"
#include "../../include/net/gen/eth_io.h"
#include "../../include/net/gen/in.h"

#include <getopt.h>

#include "../macros.h"
#include "../debug.h" 

// #define ETH_FRAME_LEN           1518
#define Address                 unsigned long

#define DEVTAP "/dev/net/tun"
#define ETH_M3IPC 		0xFD
#define IP_M3IPC 		0x1234





