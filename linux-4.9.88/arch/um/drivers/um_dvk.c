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

#define DEBUG 1

#include <kern_util.h>

#include "/usr/src/dvs/include/com/dvs_config.h"
#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/include/com/com.h"
#include "/usr/src/dvs/include/com/const.h"
#include "/usr/src/dvs/include/com/cmd.h"
#include "/usr/src/dvs/include/com/types.h"
#include "/usr/src/dvs/include/com/timers.h"
#include "/usr/src/dvs/include/com/ipc.h"
#include "/usr/src/dvs/include/com/endpoint.h"
#include "/usr/src/dvs/include/com/priv_usr.h"
//#include "/usr/src/dvs/include/com/priv.h"
#include "/usr/src/dvs/include/com/dc_usr.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/proc_sts.h"
#include "/usr/src/dvs/include/com/proc_usr.h"
//#include "/usr/src/dvs/include/com/proc.h"
#include "/usr/src/dvs/include/com/proxy_sts.h"
#include "/usr/src/dvs/include/com/proxy_usr.h"
#include "/usr/src/dvs/include/com/dvs_usr.h"
#include "/usr/src/dvs/include/com/dvk_calls.h"
#include "/usr/src/dvs/include/com/dvk_ioctl.h"
#include "/usr/src/dvs/include/com/dvs_errno.h"
#include "/usr/src/dvs/include/dvk/dvk_ioparm.h"
//#include "/usr/src/dvs/include/com/proxy.h"
#include "/usr/src/dvs/include/com/node_usr.h"
#include "/usr/src/dvs/include/com/stub_dvkcall.h"

#include "/usr/src/dvs/dvk-mod/dvk_debug.h"
#include "/usr/src/dvs/dvk-mod/dvk_macros.h"

#ifndef DVK_MAJOR
#define DVK_MAJOR 33   /* dynamic major by default */
#endif
#define UML_DVK_DEV "/dev/dvk"
#define DEVICE_NAME "dvk"


#define UMK_KERNEL_NR	(-2)
#define DCID0			0


int  dvk_fd; 
static char *dvk_dev = UML_DVK_DEV;
extern int userspace_pid[];


#define uml_bind(dcid,endpoint) 			uml_bind_X(SELF_BIND, dcid, (-1), endpoint, LOCALNODE)
#define uml_lclbind(dcid,pid,endpoint) 		uml_bind_X(LCL_BIND, dcid, pid, endpoint, LOCALNODE)


long uml_bind_X(int cmd, int dcid, int pid, int endpoint, int nodeid)
{
    long ret;
	parm_bind_t parm;
	
    DVKDEBUG(DBGPARAMS, "cmd=%d dcid=%d pid=%d endpoint=%d nodeid=%d\n", 
		cmd, dcid, pid, endpoint, nodeid);
	
	parm.parm_cmd	= cmd;	
	parm.parm_dcid	= dcid;	
	parm.parm_pid	= pid;	
	parm.parm_ep	= endpoint;	
	parm.parm_nodeid= nodeid;	
	ret = os_ioctl_generic(dvk_fd,DVK_IOCSDVKBIND, (int) &parm);
    DVKDEBUG(DBGPARAMS,"os_ioctl_generic ret=%d\n",ret);	

	return(ret);
}


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
	dvs_usr_t *dvsu_ptr;
	parm_bind_t *bind_ptr;

#ifdef DEBUG
	kernel_param_lock(THIS_MODULE);
	int uml_pid = userspace_pid[0];
	int uml_vpid = get_current_pid();
	DVKDEBUG(DBGPARAMS,"uml_pid=%d uml_vpid=%d cmd=%X arg=%X\n", uml_pid, uml_vpid, cmd, arg);
	kernel_param_unlock(THIS_MODULE);
#endif

	if( cmd == DVK_IOCGGETDVSINFO){
		dvsu_ptr = to_phys(arg);
		DVKDEBUG(DBGPARAMS,"uml_pid=%d uml_vpid=%d cmd=%X arg=%X\n", uml_pid, uml_vpid, cmd, arg);
//		dvsu_ptr = kmalloc(sizeof(dvs_usr_t), GFP_KERNEL);
//		if (dvsu_ptr == NULL)
//			return -ENOMEM;
		rcode = os_ioctl_generic(file->private_data, cmd, dvsu_ptr);
//		copy_to_user( (void *) arg, dvsu_ptr, sizeof(dvs_usr_t));
//		kfree(dvsu_ptr);
	} else if ( cmd == DVK_IOCSDVKBIND){ 
//		bind_ptr = to_phys(arg);
		bind_ptr = arg;
		DVKDEBUG(DBGPARAMS,"uml_pid=%d uml_vpid=%d cmd=%X arg=%X\n", uml_pid, uml_vpid, cmd, arg);
		DVKDEBUG(DBGPARAMS, "BEFORE cmd=%d dcid=%d pid=%d endpoint=%d nodeid=%d\n", 
			bind_ptr->parm_cmd, bind_ptr->parm_dcid, bind_ptr->parm_pid, bind_ptr->parm_ep, bind_ptr->parm_nodeid);
		bind_ptr->parm_cmd  = LCL_BIND;
		bind_ptr->parm_dcid = DCID0;
		bind_ptr->parm_pid  = uml_pid;
		bind_ptr->parm_nodeid   = LOCALNODE;
		DVKDEBUG(DBGPARAMS, "AFTER cmd=%d dcid=%d pid=%d endpoint=%d nodeid=%d\n", 
			bind_ptr->parm_cmd, bind_ptr->parm_dcid, bind_ptr->parm_pid, bind_ptr->parm_ep, bind_ptr->parm_nodeid);		
		rcode = os_ioctl_generic(file->private_data, cmd, dvsu_ptr);
	} else {
		rcode = os_ioctl_generic(file->private_data, cmd, bind_ptr);
	}
#ifdef DEBUG
	kernel_param_lock(THIS_MODULE);
	DVKDEBUG(INTERNAL,"rcode=%d\n",rcode);
	kernel_param_unlock(THIS_MODULE);
#endif
	return rcode;
}

static int uml_dvk_open(struct inode *inode, struct file *file)
{
	int rcode;
	int ep;
	int r = 0, w = 0;

	kernel_param_lock(THIS_MODULE);
	mutex_lock(&uml_dvk_mutex);
	DVKDEBUG(DBGPARAMS,"dvk_dev=%s\n",dvk_dev);
	// int os_open_file(const char *file, struct openflags flags, int mode);
	if (file->f_mode & FMODE_READ)
		r = 1;
	if (file->f_mode & FMODE_WRITE)
		w = 1;
	
	dvk_fd = os_open_file(dvk_dev, of_set_rw(OPENFLAGS(), r, w), 0);
	DVKDEBUG(INTERNAL,"dvk_fd=%d\n",dvk_fd);
	mutex_unlock(&uml_dvk_mutex);
	kernel_param_unlock(THIS_MODULE);

	if (dvk_fd < 0) {
		return dvk_fd;
	}
	file->private_data = dvk_fd;

    ep = uml_bind(DCID0, UMK_KERNEL_NR);
	DVKDEBUG(INTERNAL,"dvk_bind ep=%d\n",ep);

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
