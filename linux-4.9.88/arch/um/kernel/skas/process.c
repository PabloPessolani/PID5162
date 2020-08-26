/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <as-layout.h>
#include <kern.h>
#include <os.h>
#include <skas.h>

extern void start_kernel(void);

#ifdef CONFIG_UML_DVK
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

#include "../../include/uml_debug.h"
#include "../../include/uml_macros.h"
extern int dvk_fd;
extern int local_nodeid;
extern int uml_ep;
extern int dcid;

#ifdef ANULADO
long uml_getdvsinfo(dvs_usr_t *dvsu_ptr)
{
    long ret;
    UMLDEBUG("\n");
	ret = os_ioctl_generic(dvk_fd,DVK_IOCGGETDVSINFO, (int) dvsu_ptr);
    UMLDEBUG("os_ioctl_generic ret=%d\n",ret); 
	if (ret < 0) ERROR_RETURN(ret);
	return(ret);
}

long uml_bind_X(int cmd, int dcid, int pid, int endpoint, int nodeid)
{
    long ret;
	parm_bind_t parm;
	
    UMLDEBUG("cmd=%d dcid=%d pid=%d endpoint=%d nodeid=%d\n", 
		cmd, dcid, pid, endpoint, nodeid);
	
	parm.parm_cmd	= cmd;	
	parm.parm_dcid	= dcid;	
	parm.parm_pid	= pid;	
	parm.parm_ep	= endpoint;	
	parm.parm_nodeid= nodeid;	
	ret = os_ioctl_generic(dvk_fd,DVK_IOCSDVKBIND, (int) &parm);
    UMLDEBUG("os_ioctl_generic ret=%d\n",ret);	
	if ( ret < EDVSERRCODE) ERROR_RETURN(ret); 
	return(ret);
}
#endif //  ANULADO

#endif // CONFIG_UML_DVK



static int __init start_kernel_proc(void *unused)
{
	int pid, tid;
	long ep;

	block_signals();
	
	int rcode;
	dvs_usr_t *dvsu_ptr, dvs;

	pid = os_getpid();
	tid = os_gettid();
	UMLDEBUG("UML-kernel PID=%d TID=%d uml_ep=%d\n", pid, tid, uml_ep);
	os_flush_stdout();
	
#ifdef CONFIG_UML_DVK_ANULADO
#define UML_DVK_DEV "/dev/dvk"
	dvk_fd = (-1);
	dvk_fd = os_open_file(UML_DVK_DEV, of_set_rw(OPENFLAGS(), 1, 1), 0);
	if ( dvk_fd < 0){
		ERROR_PRINT(dvk_fd);
	}else{
		UMLDEBUG("DVK device file successfully opened!! dvk_fd=%d\n", dvk_fd);
//		printf("DVK device file successfully opened!! dvk_fd=%d\n", dvk_fd);
	}
	os_flush_stdout();
	
//	dvsu_ptr = &dvs; 
//	local_nodeid = uml_getdvsinfo(dvsu_ptr);
//	if (local_nodeid < 0) ERROR_PRINT(local_nodeid);
	
//	ep = uml_bind_X(SELF_BIND, dcid, tid, uml_ep, LOCALNODE);
	ep = dvk_bind_X(SELF_BIND, dcid, tid, uml_ep, LOCALNODE);
	if( ep != uml_ep) ERROR_PRINT(ep);
#endif // CONFIG_UML_DVK_ANULADO
		
	cpu_tasks[0].pid = pid;
	cpu_tasks[0].task = current;

	start_kernel();
	return 0;
}

extern int userspace_pid[];

extern char cpu0_irqstack[];

int __init start_uml(void)
{
	stack_protections((unsigned long) &cpu0_irqstack);
	set_sigstack(cpu0_irqstack, THREAD_SIZE);

	init_new_thread_signals();

	init_task.thread.request.u.thread.proc = start_kernel_proc;
	init_task.thread.request.u.thread.arg = NULL;


	return start_idle_thread(task_stack_page(&init_task),
				 &init_task.thread.switch_buf);
}

unsigned long current_stub_stack(void)
{
	if (current->mm == NULL)
		return 0;

	return current->mm->context.id.stack;
}
