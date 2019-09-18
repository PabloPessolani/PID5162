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

/* IOVEC */
#define IOVEC_NR        16
typedef struct iovec_dat
{
  iovec_t iod_iovec[IOVEC_NR];
  int iod_iovec_s;
  int iod_proc_nr;
  vir_bytes iod_iovec_addr;
} iovec_dat_t;



#define EC_FORMAT 		"port_name=%s if_name=%s fd=%d ifr_ifindex=%d flags=%X mode=%X ec_port=%d ec_owner=%d \n"
#define EC_FIELDS(p) 	p->port_name,p->if_name, p->ec_fd, p->ifr.ifr_ifindex, p->flags, p->mode, p->ec_port, p->ec_owner

/* Use 2^4=16 {Rx,Tx} buffers */
#define ETH_LOG_RX_BUFFERS    4
#define RX_RING_SIZE            (1 << (ETH_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK        (RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS        ((ETH_LOG_RX_BUFFERS) << 29)

#define ETH_LOG_TX_BUFFERS    4
#define TX_RING_SIZE            (1 << (ETH_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK        (TX_RING_SIZE - 1)
#define TX_RING_LEN_BITS        ((ETH_LOG_TX_BUFFERS) << 29)

/* for eth_interface */
struct eth_init_block
{
  unsigned short  mode;
  unsigned char   phys_addr[6];
  unsigned long   filter[2];
  Address         rx_ring;
  Address         tx_ring;
};

struct eth_rx_head
{
  union {
    Address         base;
    unsigned char   addr[4];
  } u;
  short           buf_length;     /* 2s complement */
  short           msg_length;
};

struct eth_tx_head
{
  union {
    Address         base;
    unsigned char   addr[4];
  } u;
  short           buf_length;     /* 2s complement */
  short           misc;
};

typedef struct eth_interface
{
  struct eth_init_block init_block;
  struct eth_rx_head    rx_ring[RX_RING_SIZE];
  struct eth_tx_head    tx_ring[TX_RING_SIZE];
  unsigned char         rbuf[RX_RING_SIZE][ETH_FRAME_LEN];
  unsigned char         tbuf[TX_RING_SIZE][ETH_FRAME_LEN];
} eth_interface_t;

/* ====== ethernet card info. ====== */
typedef struct eth_card
{
  /* ####### MINIX style ####### */
  char port_name[sizeof("eth_card#n")];
  char if_name[sizeof("eth#n")];
  struct ifreq ifr;
  int mtu;
  int flags;
  int mode;
  int transfer_mode;
  eth_stat_t eth_stat;
  iovec_dat_t read_iovec;
  iovec_dat_t write_iovec;
  iovec_dat_t tmp_iovec;
  vir_bytes write_s;
  vir_bytes read_s;
  int client;
  message sendmsg;

  eth_interface_t ec_iface;
  
//  ip_addr_t ip_addr;
//  ip_addr_t netmask;
//  ip_addr_t gw;
  
  /* ######## device info. ####### */
  port_t ec_port;
  phys_bytes ec_linmem;
  int ec_irq;
  int ec_int_pending;
  int ec_hook;
  int ec_fd;		/* TAP device file descriptor */
  int ec_state;
  int ec_owner;
  
  int ec_ramsize;
  /* PCI */
  u8_t ec_pcibus;	
  u8_t ec_pcidev;	
  u8_t ec_pcifunc;	
 
  /* Addrassing */
  u16_t ec_memseg;
  vir_bytes ec_memoff;
  
  mnx_ethaddr_t mac_address;
} eth_card_t;

/* supported max number of ether cards */
#define EC_PORT_NR_MAX 1		// sizeof(unsigned long)

/* macros for 'mode' */
#define EC_DISABLED    0x0
#define EC_SINK        0x1
#define EC_ENABLED     0x2

/* macros for 'flags' */
#define ECF_EMPTY       0x000
#define ECF_PACK_SEND   0x001
#define ECF_PACK_RECV   0x002
#define ECF_SEND_AVAIL  0x004
#define ECF_READING     0x010
#define ECF_PROMISC     0x040
#define ECF_MULTI       0x080
#define ECF_BROAD       0x100
#define ECF_ENABLED     0x200
#define ECF_STOPPED     0x400

/* === macros for ether cards (our generalized version) === */
#define EC_ISR_RINT     0x0001
#define EC_ISR_WINT     0x0002
#define EC_ISR_RERR     0x0010
#define EC_ISR_WERR     0x0020
#define EC_ISR_ERR      0x0040
#define EC_ISR_RST      0x0100

#define RECEIVE_TIMEOUT 	30
#include "glo.h"





