
#ifdef CONFIG_UML_DVK 

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

#include "../../../../ipc/dvk-mod/dvk_debug.h"
#include "../../../../ipc/dvk-mod/dvk_macros.h"

#include "../include/uml_debug.h"
#include "../include/uml_macros.h"

#ifndef DVK_MAJOR
#define DVK_MAJOR 33   /* dynamic major by default */
#endif
#define UML_DVK_DEV "/dev/dvk"
#define DEVICE_NAME "dvk"

#define c_timeout		c_flags

int dvk_thread(void *arg);
int start_dvk_thread(unsigned long sp, int *fd_out);
int os_sem_init(sem_t *sem, unsigned int value);
int os_sem_post(sem_t *sem);
int os_sem_wait(sem_t *sem);
int down_kernel(void);
int down_thread(void);
int up_kernel(void);
int up_thread(void);


#endif // CONFIG_UML_DVK
