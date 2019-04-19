
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN int local_nodeid;
EXTERN dc_usr_t *dc_ptr;
EXTERN int tracker_flag;

#define BUFF_SIZE		MAXCOPYBUF
#define MAX_MESSLEN     (BUFF_SIZE+1024)
#define MAX_VSSETS      NR_DCS
#define MAX_MEMBERS     NR_NODES

EXTERN char 			Spread_name[80];
EXTERN sp_time 			trk_timeout;
EXTERN pthread_t 		trk_thread;
EXTERN mailbox			trk_mbox;
EXTERN char 			trk_sp_group[MAXNODENAME];
EXTERN char    			trk_mbr_name[80];
EXTERN char    			trk_priv_group[MAX_GROUP_NAME];
EXTERN membership_info 	trk_memb_info;
EXTERN vs_set_info     	trk_vssets[MAX_VSSETS];
EXTERN unsigned int    	trk_my_vsset_index;
EXTERN int             	trk_num_vs_sets;
EXTERN char            	trk_members[MAX_MEMBERS][MAX_GROUP_NAME];
EXTERN char			   	trk_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
EXTERN int		   		trk_sp_nr_mbrs;
EXTERN char			 	trk_mess_in[MAX_MESSLEN];

#ifdef DVK_GLOBAL_HERE
pthread_mutex_t trk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  trk_cond = PTHREAD_COND_INITIALIZER;
#else
extern pthread_mutex_t trk_mutex;
extern pthread_cond_t  trk_cond;
#endif