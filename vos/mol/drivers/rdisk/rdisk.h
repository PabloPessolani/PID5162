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

#define BUFF_SIZE		MAXCOPYBUF
#define MAX_MESSLEN     (BUFF_SIZE+1024)
#define MAX_VSSETS      10
#define MAX_MEMBERS     NR_NODES
#define NR_DEVS         2		/* number of minor devices - example*/

/*Spread message: message m3-ipc, transfer data*/
// typedef struct {message msg; unsigned buffer_data[BUFF_SIZE];} SP_message;	 
typedef struct {
	message msg; 
	struct{
		int flag_buff;	/*compress or uncompress data into buffer*/
		long buffer_size; /* bytes compress or uncompress */
		unsigned buffer_data[BUFF_SIZE];
		} buf;
	}SP_message;	 

SP_message msg_lz4cd; 

#include "rdisk_usr.h"
#include "driver.h"
#include "sp.h"
#include "proto.h"
#include "md5.h"
#include "glo.h"

#include "../macros.h"
#include "../debug.h" 

#define SET_BIT(bitmap, bit_nr)    (bitmap |= (1 << bit_nr))
#define CLR_BIT(bitmap, bit_nr)    (bitmap &= ~(1 << bit_nr))
#define TEST_BIT(bitmap, bit_nr)   (bitmap & (1 << bit_nr))

/*For m_dtab*/
#define OPER_NAME 0
#define OPER_OPEN 1
#define OPER_NOP 2
#define OPER_IOCTL 3
#define OPER_PREPARE 4
#define OPER_TRANSF 5
#define OPER_CLEAN 6
#define OPER_GEOM 7
#define OPER_SIG 8
#define OPER_ALARM 9
#define OPER_CANC 10
#define OPER_SEL 11

/* MULTICAST MESSAGE TYPES */
#define		STS_DISCONNECTED	-1
#define RDISK_MULTICAST		0x80	
#define MC_STATUS_INFO     (RDISK_MULTICAST + 1)
#define MC_SYNCHRONIZED    (RDISK_MULTICAST + 2)

#define		STS_SYNCHRONIZED	0
#define	    STS_NEW				1
#define		STS_WAIT4PRIMARY	2
#define		STS_WAIT4SYNC		3
#define		STS_LEAVE			4

#define NO_PRIMARY			(-1)

#define REPLICATE_NO		0
#define REPLICATE_YES		1
#define VOLATILE_NO			0
#define VOLATILE_YES		1
#define REPLICATED_NO		0
#define REPLICATED_YES		1
#define ACTIVE_NO			0
#define ACTIVE_YES			1
#define DYNAMIC_NO			0
#define DYNAMIC_YES 		1
#define COMPRESS_NO			0
#define COMPRESS_YES 		1
#define AVAILABLE_NO		0
#define AVAILABLE_YES 		1
#define YES					1
#define NO					0
#define UPDATE_DIFF			0
#define UPDATE_FULL			1

#define FILE_IMAGE		0
#define MEMORY_IMAGE	1
#define NBD_IMAGE		2

#define RDISK_TIMEOUT_SEC	5
#define RDISK_TIMEOUT_MSEC	0

_PROTOTYPE( char *m_name, (void) 				);
_PROTOTYPE( struct device *m_prepare, (int device) 		);
_PROTOTYPE( int m_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) 	);
_PROTOTYPE( int m_do_open, (struct driver *dp, message *m_ptr));
_PROTOTYPE( int m_do_close, (struct driver *dp, message *m_ptr));
_PROTOTYPE( int m_init, (void) );
_PROTOTYPE( void m_geometry, (struct partition *entry));
_PROTOTYPE( int do_nop, (struct driver *dp, message *m_ptr));
_PROTOTYPE( void lz4_data_cd, (unsigned * in_buffer, size_t inbuffer_size, int flag_in));
_PROTOTYPE( void parse_config, (char *f_conf));

#define RDISK_FORMAT "nr_nodes=%d nr_sync=%d nr_radar=%d bm_nodes=%X bm_sync=%X bm_radar=%X\n"
#define RDISK_FIELDS nr_nodes, nr_sync, nr_radar, bm_nodes, bm_sync, bm_radar




