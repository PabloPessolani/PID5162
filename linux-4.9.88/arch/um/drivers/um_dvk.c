/*
 * Copyright (C) 2019 Pablo Pessolani
 * Licensed under the GPL
 * in linux-4.9.88/arch/um/drivers
 */
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <init.h>
#include <os.h>

#ifdef CONFIG_UML_DVK
#include <kern_util.h>

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

#include "/usr/src/dvs/dvk-mod/dvk_debug.h"
#include "/usr/src/dvs/dvk-mod/dvk_macros.h"

#include "/usr/src/dvs/dvk-lib/stub_dvkcall.c"

#ifndef DVK_MAJOR
#define DVK_MAJOR 33   /* dynamic major by default */
#endif
#define UML_DVK_DEV "/dev/dvk"
#define DEVICE_NAME "dvk"

extern int  dvk_fd; 
extern int local_nodeid;
extern int userspace_pid[];

char *dvk_dev = UML_DVK_DEV;
dvs_usr_t 	dvs;
dc_usr_t 	dcu;
int 		dcid; 
int 		uml_ep;
proc_usr_t 	uml_proc;

#define DVK_HELP \
"    This is used to specify the host dvk device to the dvk driver.\n" \
"    The default is \"" UML_DVK_DEV "\".\n\n"


module_param(dvk_dev, charp, 0644);
MODULE_PARM_DESC(dvk_dev, DVK_HELP);


#ifndef MODULE
static int set_dvk(char *name, int *add)
{
	DVKDEBUG(DBGPARAMS,"name=%s\n",name);
	dvk_dev = name;
	return 0;
}
__uml_setup("dvk_dev=", set_dvk, "dvk_dev=<dvk_device>\n" DVK_HELP);
#endif //MODULE

static DEFINE_MUTEX(uml_dvk_mutex);

/* /dev/dvk file operations */
static long uml_dvk_ioctl(struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	int rcode;
	void *ptr; 

	kernel_param_lock(THIS_MODULE);
	int uml_pid = userspace_pid[0];
	int uml_vpid = get_current_pid();
	DVKDEBUG(DBGPARAMS,"uml_pid=%d uml_vpid=%d cmd=%X arg=%X\n", uml_pid, uml_vpid, cmd, arg);
	kernel_param_unlock(THIS_MODULE);

	rcode = os_ioctl_generic(file->private_data, cmd, arg);

	kernel_param_lock(THIS_MODULE);
	DVKDEBUG(INTERNAL,"rcode=%d\n",rcode);
	kernel_param_unlock(THIS_MODULE);
	return rcode;
}

static int uml_dvk_open(struct inode *inode, struct file *file)
{
	int rcode;
	int ep;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *proc_ptr;

	kernel_param_lock(THIS_MODULE);
	mutex_lock(&uml_dvk_mutex);
	
	DVKDEBUG(DBGPARAMS,"dvk_dev=%s\n",dvk_dev);
	
	rcode = dvk_open();
	if( rcode < 0)
		ERROR_RETURN(rcode);

	mutex_unlock(&uml_dvk_mutex);
	kernel_param_unlock(THIS_MODULE);

	file->private_data = dvk_fd;

	dvsu_ptr = &dvs;
	local_nodeid = dvk_getdvsinfo(dvsu_ptr);
	if( local_nodeid < 0)
		ERROR_RETURN(local_nodeid);
	DVKDEBUG(INTERNAL,"local_nodeid=%d\n",local_nodeid);	
	DVKDEBUG(INTERNAL, DVS_USR_FORMAT, DVS_USR_FIELDS(dvsu_ptr));	
	
	dcu_ptr = &dcu;
	rcode = dvk_getdcinfo(dcid, dcu_ptr);
	if( rcode < 0)
		ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));	
	DVKDEBUG(INTERNAL, DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));	
	
	ep = dvk_bind(dcid, uml_ep);
	DVKDEBUG(INTERNAL,"uml_bind uml_ep=%d ep=%d\n",uml_ep, ep);
	if( ep < EDVSERRCODE)
		ERROR_RETURN(ep);
		
	proc_ptr = &uml_proc;
	rcode = dvk_getprocinfo(dcid, uml_ep, proc_ptr);
	if(rcode < 0)
		ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(proc_ptr));	

	return 0;
}

static int uml_dvk_release(struct inode *inode, struct file *file)
{
	DVKDEBUG(DBGPARAMS,"dvk_dev=%s\n",dvk_dev);
	os_close_file(file->private_data);
	return 0;
}


/* kernel module operations */
static const struct file_operations uml_dvk_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl	= uml_dvk_ioctl,
	.mmap           = NULL,
	.open           = uml_dvk_open,
	.release        = uml_dvk_release,
};

int module_dvk;

MODULE_AUTHOR("Pablo Pessolani - UTN FRSF");
MODULE_DESCRIPTION("Distributed Virtualization Kernel UML Relay");
MODULE_LICENSE("GPL");

static int __init uml_dvk_init_module(void)
{
	int kpid, ktid;
	
	printk("UML Distributed Virtualization Kernel (host dvk_dev = %s)\n",dvk_dev);

	kernel_param_lock(THIS_MODULE);
	DVKDEBUG(INTERNAL, "UML Distributed Virtualization Kernel (host dvk_dev = %s)\n",dvk_dev);
	kernel_param_unlock(THIS_MODULE);
	
	kpid = os_getpid();
	ktid = os_getpid();
	DVKDEBUG(INTERNAL, "UML-kernel PID=%d  UML-kernel TID=%d\n", kpid, ktid);

	module_dvk = register_chrdev(DVK_MAJOR, DEVICE_NAME, &uml_dvk_fops);
	if (module_dvk < 0) {
		printk(KERN_ERR "dvk: couldn't register DVK device!\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit uml_dvk_cleanup_module (void)
{
	unregister_chrdev(DVK_MAJOR, DEVICE_NAME);
}

module_init(uml_dvk_init_module);
module_exit(uml_dvk_cleanup_module);

#endif // CONFIG_UML_DVK
