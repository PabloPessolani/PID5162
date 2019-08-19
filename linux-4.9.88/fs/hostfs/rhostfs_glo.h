
#ifdef CONFIG_RHOSTFS

#ifdef RHOSTFS_GLOBAL_HERE
#define EXTERN
#else   // RHOSTFS_GLOBAL_HERE
#define EXTERN extern
#endif // RHOSTFS_GLOBAL_HERE

extern dvs_usr_t 	dvs;
extern int  		dvk_fd; 
extern int 			local_nodeid;
extern dc_usr_t 	dcu;
extern int 			dcid; 
extern char 		*dvk_dev;

EXTERN proc_usr_t 	rhc_proc;
EXTERN proc_usr_t 	rhs_proc;
EXTERN int 			rhc_ep;		// RHOSTFS Client ENDPOING :  the lowest (free) USER endpoint 
EXTERN int 			rhs_ep;		// RHOSTFS Endpoint (external from UML) from boot parameter
EXTERN int			rmt_errno;
EXTERN char rootpath[PATH_MAX+1]; 

#endif // CONFIG_RHOSTFS

