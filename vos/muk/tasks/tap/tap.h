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

#include <getopt.h>

#include "../macros.h"
#include "../debug.h" 

/* macros for 'mode' */
#define TAP_DISABLED    0x0
#define TAP_SINK        0x1
#define TAP_ENABLED     0x2

/* macros for 'flags' */
#define TAPF_EMPTY       0x000
#define TAPF_PACK_SEND   0x001
#define TAPF_PACK_RECV   0x002
#define TAPF_SEND_AVAIL  0x004
#define TAPF_READING     0x010
#define TAPF_PROMISC     0x040
#define TAPF_MULTI       0x080
#define TAPF_BROAD       0x100
#define TAPF_ENABLED     0x200
#define TAPF_STOPPED     0x400

/* === macros for ether cards (our generalized version) === */
#define TAP_ISR_RINT     0x0001
#define TAP_ISR_WINT     0x0002
#define TAP_ISR_RERR     0x0010
#define TAP_ISR_WERR     0x0020
#define TAP_ISR_ERR      0x0040
#define TAP_ISR_RST      0x0100


#ifndef __TAPIF_H__
#define __TAPIF_H__

#include "../../include/net/gen/ether.h"
#include "../../include/net/gen/in.h"
#include "../../include/net/gen/eth_hdr.h"

/* IOVEC */
#define IOVEC_NR        16
typedef struct iovec_dat
{
  iovec_t iod_iovec[IOVEC_NR];
  int iod_iovec_s;
  int iod_proc_nr;
  vir_bytes iod_iovec_addr;
} iovec_dat_t;

/* ====== tap card info. ====== */
struct  tap_card_s {
  /* ####### MINIX style ####### */
  char port_name[sizeof("tap_card#n")];
  char name[2];
  int nr;
  int flags;
  int mode;
  int transfer_mode;
// eth_stat_t tap_stat;
  iovec_dat_t read_iovec;
  iovec_dat_t write_iovec;
  iovec_dat_t tmp_iovec;
  vir_bytes write_s;
  vir_bytes read_s;
  int client;
  message sendmsg;

  mnx_ethaddr_t ethaddr;
  /* Add whatever per-interface state that is needed here. */
  int fd;
  u16_t mtu;
  u8_t hwaddr_len;

  mnx_ipaddr_t ip_addr;
  mnx_ipaddr_t netmask;
  mnx_ipaddr_t gw;
  
  pthread_t *thread;
  
};
typedef struct tap_card_s tap_card_t;

#define TAP_FORMAT "port_name=%s fd=%d\n"
#define TAP_FIELDS(p) p->port_name, p->fd 
#define ip4_addr1(ipaddr) (((u8_t*)(ipaddr))[0])
#define ip4_addr2(ipaddr) (((u8_t*)(ipaddr))[1])
#define ip4_addr3(ipaddr) (((u8_t*)(ipaddr))[2])
#define ip4_addr4(ipaddr) (((u8_t*)(ipaddr))[3])

#endif /* __TAPIF_H__ */

#ifdef ANULADO


struct netif {
  /** pointer to next in linked list */
  struct netif *next;

  /** IP address configuration in network byte order */
  ip_addr_t ip_addr;
  ip_addr_t netmask;
  ip_addr_t gw;

  /** This function is called by the network device driver
   *  to pass a packet up the TCP/IP stack. */
  netif_input_fn input;
  /** This function is called by the IP module when it wants
   *  to send a packet on the interface. This function typically
   *  first resolves the hardware address, then sends the packet. */
  netif_output_fn output;
  /** This function is called by the ARP module when it wants
   *  to send a packet on the interface. This function outputs
   *  the pbuf as-is on the link medium. */
  netif_linkoutput_fn linkoutput;
#if LWIP_NETIF_STATUS_CALLBACK
  /** This function is called when the netif state is set to up or down
   */
  netif_status_callback_fn status_callback;
#endif /* LWIP_NETIF_STATUS_CALLBACK */
#if LWIP_NETIF_LINK_CALLBACK
  /** This function is called when the netif link is set to up or down
   */
  netif_status_callback_fn link_callback;
#endif /* LWIP_NETIF_LINK_CALLBACK */
#if LWIP_NETIF_REMOVE_CALLBACK
  /** This function is called when the netif has been removed */
  netif_status_callback_fn remove_callback;
#endif /* LWIP_NETIF_REMOVE_CALLBACK */
  /** This field can be set by the device driver and could point
   *  to state information for the device. */
  void *state;
#if LWIP_DHCP
  /** the DHCP client state information for this netif */
  struct dhcp *dhcp;
#endif /* LWIP_DHCP */
#if LWIP_AUTOIP
  /** the AutoIP client state information for this netif */
  struct autoip *autoip;
#endif
#if LWIP_NETIF_HOSTNAME
  /* the hostname for this netif, NULL is a valid value */
  char*  hostname;
#endif /* LWIP_NETIF_HOSTNAME */
  /** maximum transfer unit (in bytes) */
  u16_t mtu;
  /** number of bytes used in hwaddr */
  u8_t hwaddr_len;
  /** link level hardware address of this interface */
  u8_t hwaddr[NETIF_MAX_HWADDR_LEN];
  /** flags (see NETIF_FLAG_ above) */
  u8_t flags;
  /** descriptive abbreviation */
  char name[2];
  /** number of this interface */
  u8_t num;
#if LWIP_SNMP
  /** link type (from "snmp_ifType" enum from snmp.h) */
  u8_t link_type;
  /** (estimate) link speed */
  u32_t link_speed;
  /** timestamp at last change made (up/down) */
  u32_t ts;
  /** counters */
  u32_t ifinoctets;
  u32_t ifinucastpkts;
  u32_t ifinnucastpkts;
  u32_t ifindiscards;
  u32_t ifoutoctets;
  u32_t ifoutucastpkts;
  u32_t ifoutnucastpkts;
  u32_t ifoutdiscards;
#endif /* LWIP_SNMP */
#if LWIP_IGMP
  /** This function could be called to add or delete a entry in the multicast
      filter table of the ethernet MAC.*/
  netif_igmp_mac_filter_fn igmp_mac_filter;
#endif /* LWIP_IGMP */
#if LWIP_NETIF_HWADDRHINT
  u8_t *addr_hint;
#endif /* LWIP_NETIF_HWADDRHINT */
#if ENABLE_LOOPBACK
  /* List of packets to be queued for ourselves. */
  struct pbuf *loop_first;
  struct pbuf *loop_last;
#if LWIP_LOOPBACK_MAX_PBUFS
  u16_t loop_cnt_current;
#endif /* LWIP_LOOPBACK_MAX_PBUFS */
#endif /* ENABLE_LOOPBACK */
};
#endif // ANULADO
