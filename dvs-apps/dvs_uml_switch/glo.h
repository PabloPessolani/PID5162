/* EXTERN should be extern except for the table file */
#ifdef _GLOBAL_VARS_HERE
#define EXTERN
pthread_mutex_t sw_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
#define EXTERN extern
extern pthread_mutex_t sw_mutex;
#endif

extern int local_nodeid;
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN dc_usr_t  dcu, *dc_ptr;
EXTERN int nr_rmttap;
EXTERN int nr_switch;
EXTERN switch_t switch_cfg;
EXTERN switch_t *sw_ptr;
EXTERN rmttap_t *rt_ptr;
EXTERN rmttap_t rmttap_cfg[NR_RMTTAP];
EXTERN int max_nr_rmttap;
EXTERN struct pollfd rt_fds[NR_RMTTAP];
EXTERN int rt_nfds;
EXTERN message sndr_msg;
EXTERN message rcvr_msg;
EXTERN int success_open;
