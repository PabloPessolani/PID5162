
typedef struct {
	char 			ddm_name[MAXNODENAME];
	int				ddm_nodeid;
	int				ddm_seq;

	char 			ddm_OUTCMDfifo[MAX_PIPE_NAME];
	char 			ddm_INCMDfifo[MAX_PIPE_NAME];
	int				ddm_INCMDfd;
	int				ddm_OUTCMDfd;
	
    pthread_t 		ddm_thread;
	pthread_mutex_t ddm_mutex; 	
	pthread_cond_t	ddm_cond;	

    unsigned int	ddm_bm_nodes;	// bitmap of Connected nodes
    unsigned int	ddm_bm_init;		// bitmap  initialized/active nodes 
	int				ddm_nr_nodes;
	int				ddm_nr_init;
	
    mailbox			ddm_mbox;
	int				ddm_len;
    char 			ddm_sp_group[MAXNODENAME];
    char    		ddm_mbr_name[MAX_MEMBER_NAME];
    char    		ddm_priv_group[MAX_GROUP_NAME];
    membership_info ddm_memb_info;
    vs_set_info     ddm_vssets[MAX_VSSETS];
    unsigned int    ddm_my_vsset_index;
    int             ddm_num_vs_sets;
    char            ddm_members[MAX_MEMBERS][MAX_GROUP_NAME];
    char		   	ddm_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
    int		   		ddm_sp_nr_mbrs;
    char		 	ddm_mess_in[MAX_MESSLEN];
	
} ddm_t;

#define DDM_FORMAT 	"ddm_name=%s ddm_nodeid=%d ddm_nr_nodes=%d ddm_nr_init=%d ddm_bm_nodes=%0lX ddm_bm_init=%0lX\n"
#define DDM_FIELDS(p)  p->ddm_name, p->ddm_nodeid, p->ddm_nr_nodes, p->ddm_nr_init, p->ddm_bm_nodes, p->ddm_bm_init
#define DDM1_FORMAT 	"ddm_name=%s ddm_nodeid=%d ddm_OUTfifo=%s ddm_CMDfifo=%s ddm_seq=%d \n"
#define DDM1_FIELDS(p)  p->ddm_name, p->ddm_nodeid, p->ddm_OUTfifo, p->ddm_CMDfifo, p->ddm_seq

