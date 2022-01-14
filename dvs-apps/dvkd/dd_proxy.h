typedef struct {
	char 			ddp_name[MAXNODENAME];
	int				ddp_nodeid;
	int				ddp_seq;

	char 			ddp_OUTCMDfifo[MAX_PIPE_NAME];
	char 			ddp_INCMDfifo[MAX_PIPE_NAME];
	int				ddp_INCMDfd;
	int				ddp_OUTCMDfd;
	
    pthread_t 		ddp_thread;
	pthread_mutex_t ddp_mutex; 		
	pthread_cond_t	ddp_cond;
	
    unsigned int	ddp_bm_nodes;	// bitmap of Connected nodes
    unsigned int	ddp_bm_init;		// bitmap  initialized/active nodes 
	int				ddp_nr_nodes;
	int				ddp_nr_init;
	
    mailbox			ddp_mbox;
	int				ddp_len;
    char 			ddp_sp_group[MAXNODENAME];
    char    		ddp_mbr_name[MAX_MEMBER_NAME];
    char    		ddp_priv_group[MAX_GROUP_NAME];
    membership_info ddp_memb_info;
    vs_set_info     ddp_vssets[MAX_VSSETS];
    unsigned int    ddp_my_vsset_index;
    int             ddp_num_vs_sets;
    char            ddp_members[MAX_MEMBERS][MAX_GROUP_NAME];
    char		   	ddp_sp_members[MAX_MEMBERS][MAX_GROUP_NAME];
    int		   		ddp_sp_nr_mbrs;
    char		 	ddp_mess_in[MAX_MESSLEN];
	
} ddp_t;

#define DDP_FORMAT 	"ddp_name=%s ddp_nodeid=%d ddp_nr_nodes=%d ddp_nr_init=%d ddp_bm_nodes=%0lX ddp_bm_init=%0lX\n"
#define DDP_FIELDS(p)  p->ddp_name, p->ddp_nodeid, p->ddp_nr_nodes, p->ddp_nr_init, p->ddp_bm_nodes, p->ddp_bm_init
#define DDP1_FORMAT 	"ddp_name=%s ddp_nodeid=%d ddp_OUTfifo=%s ddp_CMDfifo=%s ddp_seq=%d\n"
#define DDP1_FIELDS(p)  p->ddp_name, p->ddp_nodeid, p->ddp_OUTfifo, p->ddp_CMDfifo, p->ddp_seq

