
typedef struct {
		char 			dda_name[MAX_GROUP_NAME]; // AGENT name 
		int				dda_nodeid;				// AGENT nodeid
		int				dda_monitor;			// MONITOR nodeid
		int				dda_seq;
			
	    unsigned int	dda_bm_nodes;	// bitmap of Connected nodes
		unsigned int	dda_bm_init;		// bitmap  initialized/active nodes 
		int				dda_nr_nodes;
		int				dda_nr_init;
		
		pthread_t 		dda_thread;
		pthread_mutex_t dda_mutex;    	

		mailbox			dda_mbox;
		int				dda_len;
		char 			dda_sp_group[MAXNODENAME];
		char    		dda_mbr_name[MAX_MEMBER_NAME];
		char    		dda_priv_group[MAX_GROUP_NAME];
		membership_info dda_memb_info;
		vs_set_info     dda_vssets[MAX_VSSETS];
		unsigned int    dda_my_vsset_index;
		int             dda_num_vs_sets;
		char            dda_members[MAX_MEMBERS][MAX_GROUP_NAME];
		char		   	dda_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
		int		   		dda_sp_nr_mbrs;
		char		 	dda_mess_in[MAX_MESSLEN];	
} dda_t;
#define DDA_FORMAT 	"dda_mbr_name=%s dda_monitor=%d dda_nr_nodes=%d dda_nr_init=%d dda_bm_nodes=%0X dda_bm_init=%0X\n"
#define DDA_FIELDS(p)  p->dda_mbr_name, p->dda_monitor, p->dda_nr_nodes, p->dda_nr_init, p->dda_bm_nodes, p->dda_bm_init

