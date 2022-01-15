
/*
*Este archivo debe estar en LBAgent
*/
#include <sys/syscall.h>
//Representacion numerica de los Estados
#define LVL_UNLOADED 0
#define LVL_LOADED 1
#define LVL_SATURATED 2
//Estos son los valores techo que se van a pasar a los agentes

typedef struct {
		char 			lba_name[MAX_GROUP_NAME]; // MONITOR name 
		int				lba_monitor;			// MONITOR nodeid
		int				lba_lowwater; 		// low water load (0-100)
		int				lba_highwater;		// low water load (0-100)
		int				lba_period;			// load measurement period in seconds (1-3600)

		int				lba_load_lvl;
		int				lba_cpu_usage;
		unsigned int	lba_bm_eps;
		
	    unsigned int	lba_bm_nodes;	// bitmap of Connected nodes
		unsigned int	lba_bm_init;		// bitmap  initialized/active nodes 
		int				lba_nr_nodes;
		int				lba_nr_init;
		
		pthread_t 		lba_thread;
		pthread_mutex_t lba_mutex;    	

		mailbox			lba_mbox;
		int				lba_len;
		char 			lba_sp_group[MAXNODENAME];
		char    		lba_mbr_name[MAX_MEMBER_NAME];
		char    		lba_priv_group[MAX_GROUP_NAME];
		membership_info lba_memb_info;
		vs_set_info     lba_vssets[MAX_VSSETS];
		unsigned int    lba_my_vsset_index;
		int             lba_num_vs_sets;
		char            lba_members[MAX_MEMBERS][MAX_GROUP_NAME];
		char		   	lba_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
		int		   		lba_sp_nr_mbrs;
		char		 	lba_mess_in[MAX_MESSLEN];	
} lba_t;
#define LBA1_FORMAT 	"lba_mbr_name=%s lba_monitor=%d lba_lowwater=%d lba_highwater=%d lba_period=%d\n"
#define LBA1_FIELDS(p)  p->lba_mbr_name, p->lba_monitor, p->lba_lowwater, p->lba_highwater, p->lba_period
#define LBA2_FORMAT 	"lba_mbr_name=%s lba_monitor=%d lba_nr_nodes=%d lba_nr_init=%d lba_bm_nodes=%0X lba_bm_init=%0X\n"
#define LBA2_FIELDS(p)  p->lba_mbr_name, p->lba_monitor, p->lba_nr_nodes, p->lba_nr_init, p->lba_bm_nodes, p->lba_bm_init
#define LBA3_FORMAT 	"lba_mbr_name=%s lba_monitor=%d lba_load_lvl=%d lba_cpu_usage=%d lba_bm_eps=%0X\n"
#define LBA3_FIELDS(p)  p->lba_mbr_name, p->lba_monitor, p->lba_load_lvl, p->lba_cpu_usage, p->lba_bm_eps


