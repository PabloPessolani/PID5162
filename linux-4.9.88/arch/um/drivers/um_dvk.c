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

#include "/usr/src/dvs/include/com/config.h"
#include "/usr/src/dvs/dvk-mod/dvk_debug.h"
#include "/usr/src/dvs/dvk-mod/dvk_macros.h"

#ifndef DVK_MAJOR
#define DVK_MAJOR 33   /* dynamic major by default */
#endif
#define UML_DVK_DEV "/dev/dvk"
#define DEVICE_NAME "dvk"

static char *dvk_dev = UML_DVK_DEV;

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

#ifdef DEBUG
	kernel_param_lock(THIS_MODULE);
	DVKDEBUG(DBGPARAMS,"cmd=%X arg=%X\n",cmd, arg);
	kernel_param_unlock(THIS_MODULE);
#endif

	rcode = os_ioctl_generic(file->private_data, cmd, arg);
	
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
	
	kernel_param_lock(THIS_MODULE);
	mutex_lock(&uml_dvk_mutex);
	DVKDEBUG(DBGPARAMS,"dvk_dev=%s\n",dvk_dev);
	// int os_open_file(const char *file, struct openflags flags, int mode);
	rcode = os_open_file(dvk_dev, of_set_rw(OPENFLAGS(), r, w), 0);
	DVKDEBUG(INTERNAL,"rcode=%d\n",rcode);
	mutex_unlock(&uml_dvk_mutex);
	kernel_param_unlock(THIS_MODULE);

	if (rcode < 0) {
		return rcode;
	}
	file->private_data = rcode;
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
	kernel_param_lock(THIS_MODULE);
	DVKDEBUG(INTERNAL, "UML Distributed Virtualization Kernel (host dvk_dev = %s)\n",dvk_dev);
	kernel_param_unlock(THIS_MODULE);

	module_dvk = register_chrdev(DVK_MAYOR, DEVICE_NAME, &uml_dvk_fops);
	if (module_dvk < 0) {
		printk(KERN_ERR "dvk: couldn't register DVK device!\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit uml_dvk_cleanup_module (void)
{
	unregister_chrdev(DVK_MAYOR, DEVICE_NAME);
}

module_init(uml_dvk_init_module);
module_exit(uml_dvk_cleanup_module);

