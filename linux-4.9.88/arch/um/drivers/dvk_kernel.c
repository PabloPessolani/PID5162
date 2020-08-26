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
#include <linux/string.h>
#include <linux/syscalls.h>
#include <init.h>
#include <os.h>
  
#define DVKDBG		1

#ifdef CONFIG_UML_DVK
#include <kern_util.h>
//#include <linux/syscall.h>

#define DVK_GLOBAL_HERE		1

// DUMMY DEFINITION
typedef struct sem_s{} sem_t;

#include "um_dvk.h"
#include "glo_dvk.h"

#ifdef CONFIG_DVKIPC
	#ifdef CONFIG_DVKIOCTL
		#undef CONFIG_DVKIOCTL
	#endif //CONFIG_DVKIOCTL
#endif //CONFIG_DVKIPC

#pragma message ("Including stub_dvkcall.c")

#include "/usr/src/dvs/dvk-lib/stub_dvkcall.c"

static DEFINE_MUTEX(dvk_mutex_thread); 		// to synchronize kernel with the DVK Thread
static DEFINE_MUTEX(dvk_mutex_kernel);		// to synchronize kernel with the DVK Thread
static DEFINE_MUTEX(uml_dvk_mutex);

extern int userspace_pid[];
int module_dvk;

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


/* /dev/dvk file operations */
static long uml_dvk_ioctl(struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	int rcode;
	void *ptr; 

	kernel_param_lock(THIS_MODULE);
	int lnx_pid = os_getpid();
	int us_pid = userspace_pid[0];
	int kernel_pid = get_current_pid();
	DVKDEBUG(DBGPARAMS,"lnx_pid=%d us_pid=%d kernel_pid=%d cmd=%X arg=%X\n", 
		lnx_pid, us_pid, kernel_pid, cmd, arg);
	kernel_param_unlock(THIS_MODULE);
	
	DVKDEBUG(INTERNAL,"rcode=%d\n",rcode);
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

	mutex_unlock(&uml_dvk_mutex);
	kernel_param_unlock(THIS_MODULE);

	return 0;
}

static int uml_dvk_release(struct inode *inode, struct file *file)
{
	DVKDEBUG(DBGPARAMS,"dvk_dev=%s\n",dvk_dev);
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

MODULE_AUTHOR("Pablo Pessolani - UTN FRSF");
MODULE_DESCRIPTION("Distributed Virtualization Kernel for UML");
MODULE_LICENSE("GPL");

#ifdef ANULADO
int UML_sendrec_T(int srcdst_ep, message *m_ptr, long timeout)
{
	int rcode;
	cmd_t *cmd_ptr;
	message *msg_ptr;
	
	DVKDEBUG(INTERNAL,"srcdst_ep=%d\n", srcdst_ep);

	cmd_ptr = &dvk_cmd;
	
	cmd_ptr->c_cmd  	= CMD_SNDREC_MSG;
	cmd_ptr->c_dcid 	= dcid;
	cmd_ptr->c_src  	= uml_ep;
	cmd_ptr->c_dst  	= srcdst_ep;
	
	cmd_ptr->c_snode	= local_nodeid;	            
	cmd_ptr->c_dnode	= local_nodeid;
	cmd_ptr->c_rcode	= OK;
	cmd_ptr->c_len 	 	= 0;
	cmd_ptr->c_batch_nr = 0;
	cmd_ptr->c_timeout  = TIMEOUT_MOLCALL;
	cmd_ptr->c_snd_seq  = 0;
	cmd_ptr->c_ack_seq  = 0;
// ATENCION, ACTUALMENTE SE HACE COPIA DEL MENSAJE PERO DEBERIA HACERSE POR PUNTERO.
	memcpy(&cmd_ptr->c_u.cu_msg, m_ptr, sizeof(message));
	
	DVKDEBUG( INTERNAL, CMD_FORMAT, CMD_FIELDS(cmd_ptr));
	msg_ptr = &cmd_ptr->c_u.cu_msg;
	DVKDEBUG(INTERNAL, "Request: " MSG1_FORMAT, MSG1_FIELDS(msg_ptr));

	up_thread(); 		// wakeup  the thread 
	down_kernel();		// wait the reply
	
	if( cmd_ptr->c_rcode < 0 )
		ERROR_RETURN(cmd_ptr->c_rcode);
	return(cmd_ptr->c_rcode);
}
		

static int init_dvk_thread(void)
{
	int rcode, i;
	dvs_usr_t *dvsu_ptr;
	dc_usr_t *dcu_ptr;
	proc_usr_t *uml_ptr;  
	int my_ep;
	
	uml_pid = os_gettid();
	DVKDEBUG(INTERNAL,"uml_pid=%d\n", uml_pid);

	rcode = dvk_open();
	if( rcode < 0) ERROR_RETURN(rcode);
	
	dvsu_ptr = &dvs;

	local_nodeid = dvk_getdvsinfo(dvsu_ptr);
	if( local_nodeid < 0) ERROR_RETURN(local_nodeid);
	DVKDEBUG(INTERNAL,"local_nodeid=%d\n",local_nodeid);	
	DVKDEBUG(INTERNAL, DVS_USR_FORMAT, DVS_USR_FIELDS(dvsu_ptr));	

	dcu_ptr = &dcu;
	rcode = dvk_getdcinfo(dcid, dcu_ptr);
	if( rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));	
	DVKDEBUG(INTERNAL, DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));	
 
	rcode = dvk_lclbind(dcid, uml_pid, uml_ep);
	if(rcode != uml_ep) ERROR_RETURN(rcode);
		
	DVKDEBUG(INTERNAL,"DVK bind uml_ep=%d\n",uml_ep);
	uml_ptr = &uml_proc;
	rcode = dvk_getprocinfo(dcid, uml_ep, uml_ptr);
	if(rcode < 0) ERROR_RETURN(rcode);
	DVKDEBUG(INTERNAL, PROC_USR_FORMAT, PROC_USR_FIELDS(uml_ptr));	

	return(rcode);
}

int dvk_thread(void *arg)
{
	int rcode;
	cmd_t *cmd_ptr;
	message *msg_ptr;
	
	os_fix_helper_signals();

	// bind RDISK to DVK 
	rcode = init_dvk_thread();

	// wakeup Kernel 
	up_kernel();
	// wait the kernel wakes up me  
	down_thread();
	
	cmd_ptr = &dvk_cmd;
	while(1){
		DVKDEBUG(INTERNAL, "READY to process requests\n");	

		// Block until receive a request 
		down_thread();

		DVKDEBUG( INTERNAL, CMD_FORMAT, CMD_FIELDS(cmd_ptr));
		msg_ptr = &cmd_ptr->c_u.cu_msg;
		DVKDEBUG(INTERNAL, "Request: " MSG1_FORMAT, MSG1_FIELDS(msg_ptr));
		
		rcode = dvk_sendrec_T(cmd_ptr->c_dst, msg_ptr, cmd_ptr->c_timeout);
		cmd_ptr->c_rcode = rcode;			
		DVKDEBUG( INTERNAL, "Reply rcode=%d: " MSG1_FORMAT, rcode, MSG1_FIELDS(msg_ptr));
		up_kernel();
		os_idle_sleep(1000000000);
	}
	return 0;
}

static int dvk_driver_init(void)
{
	unsigned long stack;
	int err, dvk_pid;
	
	DVKDEBUG(INTERNAL,"\n");

	stack = alloc_stack(0, 0);
	// Start the dvk_thread (in dvk_user.c) 
	dvk_pid = start_dvk_thread(stack + PAGE_SIZE - sizeof(void *), NULL);
	printk("start_dvk_thread dvk_pid=%d\n", dvk_pid);
	if(dvk_pid < 0){
		printk(KERN_ERR "dvk_pid : Failed to start DVK thread (errno = %d) - ", dvk_pid);
		dvk_pid = -1;
		return 0;
	} 

    os_idle_sleep(2000000000);
	
	// Waits that thread wakes up me
	down_kernel();
	// Wake ups the thread 
	up_thread();
	return 0;
}
#endif // ANULADO

static int __init uml_dvk_init_module(void)
{
	int kpid, ktid, ret;

	printk("UML Distributed Virtualization Kernel (host dvk_dev = %s)\n",dvk_dev);

	kernel_param_lock(THIS_MODULE);
	DVKDEBUG(INTERNAL, "UML Distributed Virtualization Kernel (host dvk_dev = %s)\n",dvk_dev);
	kernel_param_unlock(THIS_MODULE);
	
	kpid = os_getpid();
	ktid = os_getpid();
	DVKDEBUG(INTERNAL, "UML-kernel PID=%d  UML-kernel TID=%d\n", kpid, ktid);

#ifdef ANULADO
	ret = dvk_driver_init();
	if( ret < 0) return -ENODEV;
#endif // ANULADO

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
