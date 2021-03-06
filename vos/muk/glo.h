
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

//-------------------- TAP   GLOBAL DATA 
EXTERN pthread_t tap_pth;
EXTERN proc_usr_t *tap_ptr;	
EXTERN int tap_nr, tap_ep, tap_pid;
EXTERN char *tap_cfg;

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

extern int	dvk_fd;
extern int local_nodeid;

#ifdef DVK_GLOBAL_HERE
pthread_mutex_t muk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sys_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tap_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fs_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t is_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nw_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ftp_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
extern pthread_mutex_t muk_mutex;
extern pthread_mutex_t sys_mutex;
extern pthread_mutex_t pm_mutex;
extern pthread_mutex_t rd_mutex;
extern pthread_mutex_t tap_mutex;
extern pthread_mutex_t fs_mutex;
extern pthread_mutex_t is_mutex;
extern pthread_mutex_t nw_mutex;
extern pthread_mutex_t ftp_mutex;

#endif