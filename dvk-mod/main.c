/*
 * main.c - Pablo Pessolani
 * BASE FROM the bare dvk char module  * by 
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 */

#define DVK_GLOBAL_HERE 1
#include "dvk_mod.h"

int dvk_major =   DVK_MAJOR;
int dvk_minor =   0;
int dvk_nr_devs = DVK_NR_DEVS;	

module_param(dvk_major, int, S_IRUGO);
module_param(dvk_minor, int, S_IRUGO);
module_param(dvk_nr_devs, int, S_IRUGO);

MODULE_AUTHOR("Pablo Pessolani");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("The DVK Linux module.");
MODULE_VERSION("4.9.88");


/* Init handler which is used to lookup addresses of symbols that the
 * replacement function requires but are not exported to modules */
static int exit_unbind_init_handler(void)
{
	DVKDEBUG(DBGLVL0,"\n");
	return 0;
}

/* Set up the jump structure. All that's required is:
 * from_symbol_name: name of the original kernel function that is to be
 *                   replaced
 * to_symbol_name:   name of our function that replaces the original function
 * init_handler:     pointer to an init handler that is called at
 *                   initialization time */
struct reljmp rj_exit_unbind = {
	.from_symbol_name = "old_exit_unbind",
    .to_symbol_name = "new_exit_unbind",
    .init_handler = exit_unbind_init_handler,
};

/* List of functions to replace */
struct reljmp *reljmp_func[] = {
	&rj_exit_unbind,
};

char *dvk_routine_names[DVK_NR_CALLS] = {
    "void0",
    "dc_init",
    "mini_send",
    "mini_receive",
    "mini_notify",
    "mini_sendrec",
    "mini_rcvrqst",
    "mini_reply",
    "dc_end",
    "bind",
    "unbind",
    "getpriv",
    "setpriv",
    "vcopy",
    "getdcinfo",
    "getprocinfo",
    "mini_relay",
    "proxies_bind",
    "proxies_unbind",
    "getnodeinfo",
    "put2lcl",
    "get2rmt",
    "add_node",
    "del_node",
    "dvs_init",
    "dvs_end",
    "getep",
    "getdvsinfo",
    "proxy_conn",
    "wait4bind",
    "migrate",   
    "node_up",
    "node_down",
    "getproxyinfo",
	"wakeup",
};   /* the ones we are gonna replace */

long (*dvk_io_routine[DVK_NR_CALLS])(unsigned long arg) = {
    io_void0,
    io_dc_init,
    io_mini_send,
    io_mini_receive,
    io_mini_notify,
    io_mini_sendrec,
    io_mini_rcvrqst,
    io_mini_reply,
    io_dc_end,
    io_bind,
    io_unbind,
    io_getpriv,
    io_setpriv,
    io_vcopy,
    io_getdcinfo,
    io_getprocinfo,
    io_mini_relay,
    io_proxies_bind,
    io_proxies_unbind,
    io_getnodeinfo,
    io_put2lcl,
    io_get2rmt,
    io_add_node,
    io_del_node,
    io_dvs_init,
    io_dvs_end,
    io_getep,
    io_getdvsinfo,
    io_proxy_conn,
    io_wait4bind,
    io_migrate,
    io_node_up,
    io_node_down,
    io_getproxyinfo,
    io_wakeup,
};

static int dvk_replace_init(void)
{
	int retval;
	int i;

	DVKDEBUG(DBGLVL0,"\n");

	/* Initialize the jumps */
	retval = reljmp_init_once();
	if (retval) ERROR_RETURN(retval);

	for (i = 0; i < ARRAY_SIZE(reljmp_func); i++) {
		retval = reljmp_init(reljmp_func[i]);
		if (retval) ERROR_RETURN(retval);
	}

	/* Register the jumps */
	for (i = 0; i < ARRAY_SIZE(reljmp_func); i++) {
		reljmp_register(reljmp_func[i]);
	}

	return 0;
}

static void dkv_replace_exit(void)
{
	int i;

	DVKDEBUG(DBGLVL0,"\n");
	/* Unregister the jumps */
	for (i = 0; i < ARRAY_SIZE(reljmp_func); i++) {
		reljmp_unregister(reljmp_func[i]);
	}
}

//----------------------------------------------------------
//			dvk_open
//----------------------------------------------------------
int dvk_open(struct inode *inode, struct file *filp)
{
	DVKDEBUG(DBGLVL0,"\n");
	return 0;          /* success */
}

//----------------------------------------------------------
//			dvk_read
//----------------------------------------------------------
ssize_t dvk_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	DVKDEBUG(DBGLVL0,"\n");
	return EDVSNOSYS;
}

//----------------------------------------------------------
//			dvk_write
//----------------------------------------------------------
ssize_t dvk_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	DVKDEBUG(DBGLVL0,"\n");
	return EDVSNOSYS;
}

	
//----------------------------------------------------------
//			dvk_ioctl
//----------------------------------------------------------
long dvk_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0, dvk_call;
	int rcode = 0;

	DVKDEBUG(DBGLVL0,"cmd=%X arg=%lX\n", cmd, arg);
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != DVK_IOC_MAGIC) 	return -ENOTTY;
	if (_IOC_NR(cmd) > DVK_IOC_MAXNR) 	return -ENOTTY;
	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	dvk_call = _IOC_NR(cmd);
	DVKDEBUG(DBGLVL0,"DVK_CALL=%d (%s) \n", dvk_call, dvk_routine_names[dvk_call] );

	rcode = (*dvk_io_routine[dvk_call])(arg);

	if(rcode) ERROR_RETURN(rcode);
	return(rcode);
}


//----------------------------------------------------------
//			dvk_cleanup_module
//----------------------------------------------------------
void dvk_cleanup_module(void)
{
	DVKDEBUG(DBGLVL0,"\n");
	dkv_replace_exit();
}

//----------------------------------------------------------
//			dvk_init_module
//----------------------------------------------------------
int dvk_init_module(void)
{
	int rcode=0;
static	char *tasklist_name = "tasklist_lock";
static	char *setaffinity_name = "sched_setaffinity";
static	char *free_nsproxy_name = "free_nsproxy";
static	char *sys_wait4_name = "sys_wait4";
static	char *scnprintf_name = "bitmap_scnprintf";
static	char *exit_unbind_name = "exit_unbind_ptr";
	
	unsigned long sym_addr = kallsyms_lookup_name(tasklist_name);
	
	dvs.d_dbglvl = 0xFFFFFFFF;

	tasklist_ptr = (rwlock_t *) sym_addr;
	DVKDEBUG(DBGLVL0,"Hello, DVS! tasklist_ptr=%X\n", tasklist_ptr);

	sym_addr = kallsyms_lookup_name(setaffinity_name);
	setaffinity_ptr = (void *) sym_addr;
	DVKDEBUG(DBGLVL0,"Hello, DVS! setaffinity_ptr=%X\n", setaffinity_ptr);

	sym_addr = kallsyms_lookup_name(free_nsproxy_name);
	free_nsproxy_ptr = (void *) sym_addr;
	DVKDEBUG(DBGLVL0,"Hello, DVS! free_nsproxy_ptr=%X\n", free_nsproxy_ptr);

	sym_addr = kallsyms_lookup_name(sys_wait4_name);
	sys_wait4_ptr = (void *) sym_addr;
	DVKDEBUG(DBGLVL0,"Hello, DVS! sys_wait4_ptr=%X\n", sys_wait4_ptr);
		
	sym_addr = kallsyms_lookup_name(scnprintf_name);
	scnprintf_ptr = (void *) sym_addr;
	DVKDEBUG(DBGLVL0,"Hello, DVS! scnprintf_ptr=%X\n", scnprintf_ptr);
	
	sym_addr = kallsyms_lookup_name(exit_unbind_name);
	dvk_unbind_ptr = (void *) sym_addr;
	DVKDEBUG(DBGLVL0,"Hello, DVS! dvk_unbind_ptr=%X\n", dvk_unbind_ptr);
	
	DVKDEBUG(DBGLVL0,"usage: insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 \n");
	DVKDEBUG(DBGLVL0,"parms:  dvk_major=%d dvk_minor=%d dvk_nr_devs=%d\n",
				 dvk_major, dvk_minor, dvk_nr_devs);

	/* 
	 * Register the character device (atleast try) 
	 */
	rcode = register_chrdev(dvk_major, DVK_FILE_NAME, &dvk_fops);

	/* 
	 * Negative values signify an error 
	 */
	if (rcode < 0) {
		printk(KERN_ALERT "%s failed with %d\n",
		       "Sorry, registering the character device ", rcode);
		return rcode;
	}

	DVKDEBUG(DBGLVL0, "%s The major device number is %d.\n",
	       "Registeration is a success", dvk_major);
	DVKDEBUG(DBGLVL0, "We suggest you use:\n");
	DVKDEBUG(DBGLVL0, "mknod %s c %d 0\n", DVK_FILE_NAME, dvk_major);
	DVKDEBUG(DBGLVL0, "OLD local_nodeid=%d\n", atomic_read(&local_nodeid));
//	atomic_set ( &local_nodeid, 3);
//	DVKDEBUG(DBGLVL0, "NEW local_nodeid=%d\n", atomic_read(&local_nodeid));

	rcode = dvk_replace_init();
	
	return(rcode);
}

module_init(dvk_init_module);
module_exit(dvk_cleanup_module);
