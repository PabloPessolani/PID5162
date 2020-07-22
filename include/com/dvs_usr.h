#ifndef COM_DVS_USR_H
#define COM_DVS_USR_H

#define DVS_RUNNING	0x00	/* DVS is RUNNING*/
#define DVS_NO_INIT	(-1)	/* DVS has not been initialized */

#define BIT_IPC     	0	/* The DVK is embedded in the linux kernel as a new System V IPC  */
#define BIT_IOCTL		1	/* The DVK was loaded as a kernel module */ 
#define BIT_USERSPACE	2 	/* Kernel running in user space i.e. UML */
#define BIT_INITIALIZED 8 	/* Kernel running in user space i.e. UML */

enum dvs_flags {
		DVK_IPC 		= (1<<BIT_IPC),	
		DVK_IOCTL		= (1<<BIT_IOCTL),		
		DVK_USERSPACE	= (1<<BIT_USERSPACE),	
		DVK_INITIALIZED	= (1<<BIT_INITIALIZED),	
};

struct dvs_usr {
    char 	d_name[MAXPROCNAME]; 	
	int		d_nr_dcs;
	int		d_nr_nodes;
	int 	d_nr_procs;
	int 	d_nr_tasks;
	int 	d_nr_sysprocs;
	int 	d_max_copybuf;
	int 	d_max_copylen;

	unsigned long int d_dbglvl;
	int	d_version;
	unsigned long d_flags;
	int d_size_proc;
};
typedef struct dvs_usr dvs_usr_t;

#define DVS_USR_FORMAT "d_name=%s d_nr_dcs=%d d_nr_nodes=%d d_nr_procs=%d d_nr_tasks=%d d_nr_sysprocs=%d \n"
#define DVS_USR_FIELDS(p) p->d_name, p->d_nr_dcs,p->d_nr_nodes, p->d_nr_procs, p->d_nr_tasks, p->d_nr_sysprocs  

#define DVS_MAX_FORMAT "d_max_copybuf=%d d_max_copylen=%d\n"
#define DVS_MAX_FIELDS(p) p->d_max_copybuf,p->d_max_copylen  

#define DVS_VER_FORMAT "d_dbglvl=%lX version=%d flags=%lX sizeof(proc)=%d\n"
#define DVS_VER_FIELDS(p) p->d_dbglvl, p->d_version, p->d_flags, p->d_size_proc 


#endif /* COM_DVS_USR_H */
