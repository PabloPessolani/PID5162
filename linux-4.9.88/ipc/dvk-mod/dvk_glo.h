/*----------------------------------------------------------------*/
/*				GLOBAL VARIABLES		*/
/*----------------------------------------------------------------*/

EXTERN int dvk_iface_type; 

EXTERN dc_desc_t dc[NR_DCS];		/* an array of DC structs */
EXTERN cluster_node_t node[NR_NODES];	/* an array of NODE structs */
EXTERN proxies_t proxies[NR_NODES];		/* an array of PROXY PAIR structs */
EXTERN struct kref dvs_kref;		/* reference counter for DVS */
EXTERN int sizeof_proc_aligned; 	/* size of the struct proc cache line aligned */
EXTERN int log2_proc_size;		/* number of bits to left shift 			 */

EXTERN struct proc_dir_entry *info_entry;
EXTERN struct proc_dir_entry *proc_dvs_dir;
EXTERN struct proc_dir_entry *nodes_entry;
EXTERN struct proc_dir_entry *proxies_dir;
EXTERN struct proc_dir_entry *proxies_info_entry;
EXTERN struct proc_dir_entry *proxies_procs_entry;
EXTERN struct dentry *dbg_dvs; /* directory entry of "dvs" directory on debugfs */

EXTERN rwlock_t *tasklist_ptr;
EXTERN long (*setaffinity_ptr)(pid_t , const struct cpumask *); 
EXTERN void (*free_nsproxy_ptr)(struct nsproxy *ns);
EXTERN asmlinkage long (*sys_wait4_ptr)(pid_t pid, int __user *stat_addr,
				int options, struct rusage __user *ru);
				
extern asmlinkage long (*exit_unbind_ptr) (long code);

EXTERN asmlinkage long (*dvk_unbind_ptr)(long code);	 
EXTERN long (*dvk_mod_ipc_ptr) (int call, int first, unsigned long second,unsigned long third, void __user *ptr, long fifth);
EXTERN ssize_t dvk_vm_rw(struct task_struct *task,
			     const struct iovec *lvec,
			     unsigned long liovcnt,
			     const struct iovec *rvec,
			     unsigned long riovcnt,
			     unsigned long flags, int vm_write);

//EXTERN int dvk_mod_loaded; 
		 
extern atomic_t	local_nodeid; 

#ifdef DVK_GLOBAL_HERE

char *dvk_names[DVK_NR_CALLS] = {
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

struct file_operations nodes_file_fops= {
 owner : THIS_MODULE,
 read  : nodes_read,
};

struct file_operations info_file_fops = {
 owner : THIS_MODULE,
 read  : info_read,
};

struct file_operations proxies_info_file_fops = {
 owner : THIS_MODULE,
 read  : proxies_info_read,
};

struct file_operations proxies_procs_file_fops = {
 owner : THIS_MODULE,
 read  : proxies_procs_read,
};

struct file_operations dc_info_file_fops = {
 owner : THIS_MODULE,
 read  : dc_info_read,
};

struct file_operations dc_procs_file_fops = {
 owner : THIS_MODULE,
 read  : dc_procs_read,
};

struct file_operations dc_ipcto_file_fops = {
 owner : THIS_MODULE,
 read  : dc_ipcto_read,
};

struct file_operations dc_dvkcalls_file_fops = {
 owner : THIS_MODULE,
 read  : dc_dvkcalls_read,
};

struct file_operations dc_stats_file_fops = {
 owner : THIS_MODULE,
 read  : dc_stats_read,
};

struct file_operations proc_dbg_fops = {
	open : proc_dbg_open,
	release : proc_dbg_close,
	mmap : proc_dbg_mmap,
};

dvs_usr_t dvs = {
	    "DVS_IPC",
		NR_DCS,
		NR_NODES,
		NR_PROCS,
		NR_TASKS,
		NR_SYS_PROCS,
		MAXCOPYBUF,
		MAXCOPYLEN,

		(GENERIC|INTERNAL|DBGPROCLOCK|DBGDCLOCK|DBGMESSAGE|DBGCMD|DBGVCOPY|DBGPARAMS|DBGPROC|\
		 DBGPRIV|DBGPROCSEM),
		DVS_VERSION,
		DVK_IPC
		};
dvs_usr_t *dvs_ptr;

#if LOCK_DVS_TYPE == USE_DVS_RWLOCK
rwlock_t dvs_rwlock = RW_LOCK_UNLOCKED;		
#elif LOCK_DVS_TYPE == USE_DVS_MUTEX
DEFINE_MUTEX(dvs_mutex);				/* LINUX mutex to protect DVS 	*/	
#elif LOCK_DVS_TYPE == USE_DVS_RWSEM
DECLARE_RWSEM(dvs_rwsem);				/* LINUX RW Semaphore to protect DVS 	*/
#elif LOCK_DVS_TYPE == USE_DVS_RCU
spinlock_t dvs_spinlock = SPIN_LOCK_UNLOCKED;	
#endif


#else /* DVK_GLOBAL_HERE */

EXTERN char *dvk_names[DVK_NR_CALLS];

// EXTERN struct file_operations nodes_file_fops;
EXTERN struct file_operations info_file_fops;
EXTERN struct file_operations proxies_info_file_fops;
EXTERN struct file_operations proxies_procs_file_fops;
EXTERN struct file_operations dc_info_file_fops;
EXTERN struct file_operations dc_procs_file_fops;
EXTERN struct file_operations dc_stats_file_fops;
EXTERN struct file_operations dc_ipcto_file_fops;
EXTERN struct file_operations dc_dvkcalls_file_fops;

EXTERN struct file_operations proc_dbg_fops;
EXTERN dvs_usr_t dvs;
EXTERN dvs_usr_t *dvs_ptr;

#if LOCK_DVS_TYPE == USE_DVS_RWLOCK
EXTERN rwlock_t   	dvs_rwlock;
#elif LOCK_DVS_TYPE == USE_DVS_MUTEX
EXTERN struct mutex dvs_mutex;
#elif LOCK_DVS_TYPE == USE_DVS_RWSEM
EXTERN struct rw_semaphore dvs_rwsem;
#elif LOCK_DVS_TYPE == USE_DVS_RCU
EXTERN spinlock_t dvs_spinlock;
#endif

#endif /* DVK_GLOBAL_HERE */



