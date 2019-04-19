
/* This is the master header for PM.  It includes some other files
 * and defines the principal constants.
 */

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

#include <netinet/in.h>       // IPPROTO_RAW, INET_ADDRSTRLEN
#include <netinet/ip.h>       // IP_MAXPACKET (which is 65535)
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_ARP = 0x0806
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO

#define DVS_USERSPACE	1
#define _GNU_SOURCE
#include <sched.h>
#define cpumask_t cpu_set_t

//#include <net/ethernet.h>

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
#include "../../include/signal.h"
#include "../../include/slots.h"
#include "../../include/syslib.h"
#include "../../include/mollib.h"
#include "../../../../include/com/stub_dvkcall.h"

#include "../debug.h"
#include "../macros.h"

extern int	dvk_fd;
extern int local_nodeid;

typedef int ioreq_t;

#include "./generic/buf.h"
#include "./generic/event.h"
#include "./generic/assert.h"
#include "./generic/type.h"
#include "./generic/sr.h"
#include "./generic/ip.h"
#include "./generic/eth.h"

#include "../../include/ioctl.h"
#include "../../include/net/hton.h"
#include "../../include/net/ioctl.h"
#include "../../include/net/gen/in.h"
#include "../../include/net/gen/ip_hdr.h"
#include "../../include/net/gen/tcp.h"
#include "../../include/net/gen/tcp_io.h"
#include "../../include/net/gen/tcp_hdr.h"
#include "../../include/net/gen/udp.h"
#include "../../include/net/gen/udp_io.h"
#include "../../include/net/gen/udp_hdr.h"
#include "../../include/net/gen/icmp.h"
#include "../../include/net/gen/icmp_hdr.h"
#include "const.h"
#include "../../include/net/gen/ether.h"
#include "osdep_eth.h"
#include "../../include/net/gen/eth_hdr.h"
#include "../../include/net/gen/psip_hdr.h"

#include "../../include/net/gen/ip_io.h"
#include "../../include/net/gen/eth_io.h"
#include "../../include/net/gen/arp_io.h"
#include "../../include/net/gen/psip_io.h"
#include "../../include/net/gen/route.h"
#include "../../include/net/gen/oneCsum.h"

#include "./generic/clock.h"
#include "./generic/tcp_int.h"
#include "./generic/udp_int.h"
#include "./generic/eth_int.h"
#include "./generic/ipr.h"

#include "./generic/arp.h"
#include "./generic/psip.h"
#include "./generic/tcp.h"
#include "./generic/udp.h"
#include "./generic/icmp.h"
#include "./generic/icmp_lib.h"
#include "./generic/rand256.h"

#include "./sys/ioc_file.h"

#include "mq.h"
#include "inet_config.h"
#include "./generic/ip_int.h"

#include "glo.h"

#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1	/* tell headers that this is the kernel */

#define HZ 			100	/* SOLO TEMPORAL HASTA PONERLO EN FUNCION DE JIFFIES */

#define	LOCAL_SYSTASK		(SYSTEM - local_nodeid)
#define	LOCAL_SLOTS			(SYSTEM - dvs.d_nr_nodes) 

#define DBLOCK(level, code) \
	do { if ((level) & DEBUG) { where(); code; } } while(0)
#define DIFBLOCK(level, condition, code) \
	do { if (((level) & DEBUG) && (condition)) \
		{ where(); code; } } while(0)

#define offsetof(type, ident)	((mnx_size_t) (unsigned long) &((type *)0)->ident)
			
void panic0(char *file, int line);
void inet_panic(void); 

#define ip_panic(print_list)  \
	(panic0(__FILE__, __LINE__), printf print_list, panic())
#define panic() inet_panic()

#if DEBUG
#define ip_warning(print_list)  \
	( \
	    fprintf(stdout,"WARNING at %s, %d: ", __FILE__, __LINE__);\
		printf print_list;\
		fflush(stdout);\
	)
#else
#define ip_warning(print_list)	((void) 0)
#endif

		