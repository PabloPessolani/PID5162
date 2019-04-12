
#define 	ENABLE_SYSTASK	1
#define 	ENABLE_PM  		1
#define 	ENABLE_RDISK  	1
#define 	ENABLE_FS	  	1
#define 	ENABLE_IS	  	1
#define		ENABLE_NW		1
#define		ENABLE_FTP		1

EXTERN dc_usr_t  dcu, *dc_ptr;
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN node_usr_t node, *node_ptr;
EXTERN unsigned int muk_mandatory;
EXTERN char *muk_name;

#ifdef ALLOC_LOCAL_TABLE 			
EXTERN proc_usr_t *proc_table;		/* SYSTASK LOCAL process table 	*/
#else /* ALLOC_LOCAL_TABLE */			
EXTERN char     *kproc_map;		/* KERNEL memory mapped process table	*/
#endif /* ALLOC_LOCAL_TABLE */

EXTERN int local_nodeid;
EXTERN int dcid;

EXTERN int dump_fd;
EXTERN time_t boottime;

//-------------------- SYSTASK GLOBAL DATA 
EXTERN pthread_t sys_pth;
EXTERN proc_usr_t *sys_ptr;	
EXTERN int sys_nr, sys_ep, sys_pid;

//-------------------- CLOCK  GLOBAL DATA 
EXTERN int clock_ep, clock_pid;

//-------------------- PM  GLOBAL DATA 
EXTERN pthread_t pm_pth;
EXTERN proc_usr_t *pm_ptr;	
EXTERN int pm_nr, pm_ep, pm_pid;

//-------------------- RDISK  GLOBAL DATA 
EXTERN pthread_t rd_pth;
EXTERN proc_usr_t *rd_ptr;	
EXTERN int rd_nr, rd_ep, rd_pid;
EXTERN char *rd_cfg;

//-------------------- FS  GLOBAL DATA 
EXTERN pthread_t fs_pth;
EXTERN proc_usr_t *fs_ptr;	
EXTERN int fs_nr, fs_ep, fs_pid;
EXTERN char *fs_cfg;

//-------------------- IS  GLOBAL DATA 
EXTERN pthread_t is_pth;
EXTERN proc_usr_t *is_ptr;	
EXTERN int is_nr, is_ep, is_pid;
#define MAXHTMLBUF		65536
EXTERN char is_buffer[MAXHTMLBUF]; 

//-------------------- WEB  GLOBAL DATA 
EXTERN pthread_t web_pth;
EXTERN proc_usr_t *web_ptr;	
EXTERN int web_nr, web_ep, web_pid;
EXTERN char *web_cfg;

//-------------------- FTP GLOBAL DATA 
EXTERN pthread_t ftp_pth;
EXTERN proc_usr_t *ftp_ptr;	
EXTERN int ftp_nr, ftp_ep, ftp_pid;
EXTERN char *ftp_cfg;


#ifdef DVK_GLOBAL_HERE
pthread_mutex_t muk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t muk_cond = PTHREAD_COND_INITIALIZER;

pthread_cond_t sys_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t pm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t rd_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t fs_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t is_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t nw_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t ftp_cond = PTHREAD_COND_INITIALIZER;
#else
extern pthread_mutex_t muk_mutex;
extern pthread_cond_t muk_cond;

extern pthread_cond_t sys_cond;
extern pthread_cond_t pm_cond;
extern pthread_cond_t rd_cond;
extern pthread_cond_t fs_cond;
extern pthread_cond_t is_cond;
extern pthread_cond_t nw_cond;
extern pthread_cond_t ftp_cond;

#endif