
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
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN proxy_t 	proxy_tab[NR_NODES];

EXTERN sess_tab_t sess_table[NR_DCS]; // Session Table  
EXTERN int nr_sess_entries;				// Constant !! Number of session entries per table .

extern  local_nodeid;
extern char nonprog[];
EXTERN  service_t 	service_tab[MAX_SVC_NR];

EXTERN lb_t  lb;
EXTERN lb_t *lb_ptr;

