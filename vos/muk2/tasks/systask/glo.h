#ifdef DVK_GLOBAL_HERE
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN molclock_t realtime;               	/* Minix real time clock */
EXTERN long clockTicks;
EXTERN molclock_t realtime;               	/* Minix real time clock 			*/
EXTERN molclock_t next_timeout;		/* realtime that next timer expires 		*/

EXTERN long init_time;
EXTERN struct sysinfo info;
EXTERN int pagesize;
EXTERN message sys_m;

EXTERN int sys_who_e, sys_who_p;		/* message source endpoint and proc_table */

EXTERN priv_usr_t *priv_table;		/* Privileges table		*/
EXTERN int debug_fd;

EXTERN int next_child;

EXTERN 	time_t 	last_rqst;		/* time of last slot request */


