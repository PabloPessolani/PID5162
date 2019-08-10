#ifndef __UM_FS_RHOSTFS
#define __UM_FS_FHOSTFS


#include "/usr/src/linux/arch/sh/include/uapi/asm/unistd_32.h"

#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/const.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dvk_calls.h"
#include "/usr/src/dvs/include/com/dvk_ioctl.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/dvk/dvk_ioparm.h"
#include "/usr/src/dvs/include/generic/tracker.h"
#include "/usr/src/dvs/include/com/stub_dvkcall.h"

#include "/usr/src/linux/arch/sh/include/uapi/asm/unistd_32.h"

#include "rh_debug.h"
#include "rh_macros.h"

#ifndef DVK_MAJOR
#define DVK_MAJOR 33   /* dynamic major by default */
#endif
#define UML_DVK_DEV "/dev/dvk"
#define DEVICE_NAME "dvk"

#include "hostfs.h"
#include "rhostfs_proto.h"
#include "rhostfs_glo.h"

#define HOSTNAME_MAX	64
#define	TIMEOUT_RMTCALL	TIMEOUT_MOLCALL
#define RHOSTFS_PROC_NR 1

#define MAX_SYS_CALL	0x1000
#define RMT_opendir 	(MAX_SYS_CALL +1) 
#define RMT_seekdir		(MAX_SYS_CALL +2)
#define RMT_readdir		(MAX_SYS_CALL +3)
#define RMT_closedir	(MAX_SYS_CALL +4)

struct UML_dirent {
   ino_t          d_ino;       /* Inode number */
   off_t          d_off;       /* Not an offset; see below */
   unsigned short d_reclen;    /* Length of this record */
   unsigned char  d_type;      /* Type of file; not supported
								  by all filesystem types */
   char           d_name[256]; /* Null-terminated filename */
};
		   
#endif
