

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/uaccess.h>	/* copy_*_user */
#include <linux/cpumask.h>
#include <linux/syscalls.h>
#include <linux/capability.h>

#include <uapi/linux/uio.h>

#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/const.h"
#include "/usr/src/dvs/include/com/endpoint.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
#include "/usr/src/dvs/include/com/priv.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
#include "/usr/src/dvs/include/com/proc.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/proxy.h"
#include "/usr/src/dvs/include/com/macros.h"
#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dvk_calls.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"

#include "./dvk-mod/dvk_newproto.h"

#include "dvk_debug.h"
#include "dvk_macros.h"
#include "dvk_proto.h"
#include "dvk_glo.h"
//#include "dvk_ipcproto.h"

extern atomic_t local_nodeid;


