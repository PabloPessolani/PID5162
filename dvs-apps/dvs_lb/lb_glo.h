
#ifdef _TABLE
	#ifdef EXTERN
	#undef EXTERN
	#endif 
#define EXTERN
#else
	#ifdef EXTERN
	#undef EXTERN
	#endif 
#define EXTERN extern
#endif

/* The parameters of the call are kept here. */
//EXTERN message m_in;		/* the input message itself */
//EXTERN message m_out;		/* the output message used for reply */
//EXTERN message *m_ptr;		/* pointer to message */

EXTERN int local_nodeid;
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN dc_usr_t  dcu[NR_DCS], *dc_ptr[NR_DCS];

EXTERN sess_tab_t sess_table[NR_DCS]; // Session Table  
EXTERN int nr_sess_entries;				// Constant !! Number of session entries per table .

EXTERN lbpx_desc_t *lb_cltpxy[NR_NODES*2];	// Client proxy pairs 
EXTERN lbpx_desc_t *lb_svrpxy[NR_NODES*2];	// Server proxy pairs

EXTERN	char Spread_name[80];
EXTERN  sp_time lb_timeout;

EXTERN  server_t 	server_tab[NR_NODES];
EXTERN  client_t 	client_tab[NR_NODES];
EXTERN  service_t 	service_tab[MAX_SVC_NR];

EXTERN pthread_t 	lbm_thread;	// Load Balancer Monitor thread 

EXTERN clockid_t clk_id;

EXTERN lb_t  lb;
EXTERN lb_t *lb_ptr;

extern char nonprog[];

