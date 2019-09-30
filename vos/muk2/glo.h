
#define 	ENABLE_SYSTASK	1
#define 	ENABLE_PM  		1
#define 	ENABLE_RDISK  	1
#define 	ENABLE_FS	  	1
#define 	ENABLE_IS	  	1
#define		ENABLE_NW		1
#define		ENABLE_FTP		1

extern int local_nodeid;

EXTERN dc_usr_t  dcu, *dc_ptr;
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN node_usr_t node, *node_ptr;
EXTERN unsigned int muk_mandatory;
EXTERN char *muk_name;
EXTERN int dcid;
EXTERN int dump_fd;
EXTERN time_t boottime;

EXTERN int main_ep;

//-------------------- SYSTASK GLOBAL DATA 
// EXTERN Task *sys_tsk;
EXTERN muk_proc_t *sys_ptr;	
EXTERN int sys_nr, sys_ep, sys_id;

//-------------------- CLOCK  GLOBAL DATA 
EXTERN int clock_ep, clock_id;

//-------------------- PM  GLOBAL DATA 
// EXTERN Task *pm_tsk;
EXTERN muk_proc_t *pm_ptr;	
EXTERN int pm_nr, pm_ep, pm_id;

//-------------------- RDISK  GLOBAL DATA 
// EXTERN Task *rd_tsk;
EXTERN muk_proc_t *rd_ptr;	
EXTERN int rd_nr, rd_ep, rd_id;
EXTERN char *rd_cfg;

//-------------------- FS  GLOBAL DATA 
// EXTERN Task *fs_tsk;
EXTERN muk_proc_t *fs_ptr;	
EXTERN int fs_nr, fs_ep, fs_id;
EXTERN char *fs_cfg;

//-------------------- IS  GLOBAL DATA 
// EXTERN Task *is_tsk;
EXTERN muk_proc_t *is_ptr;	
EXTERN int is_nr, is_ep, is_id;
#define MAXHTMLBUF		65536
EXTERN char is_buffer[MAXHTMLBUF]; 

//-------------------- WEB  GLOBAL DATA 
// EXTERN Task *web_tsk;
EXTERN muk_proc_t *web_ptr;	
EXTERN int web_nr, web_ep, web_id;
EXTERN char *web_cfg;

//-------------------- FTP GLOBAL DATA 
// EXTERN Task *ftp_tsk;
EXTERN muk_proc_t *ftp_ptr;	
EXTERN int ftp_nr, ftp_ep, ftp_id;
EXTERN char *ftp_cfg;

EXTERN int	muk_mutex; // pseudo-mutex 

EXTERN Rendez muk_cond;
EXTERN Rendez sys_cond;
EXTERN Rendez pm_cond;
EXTERN Rendez rd_cond;
EXTERN Rendez fs_cond;
EXTERN Rendez is_cond;
EXTERN Rendez nw_cond;
EXTERN Rendez ftp_cond;
