
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
EXTERN proxy_t 	proxy_tab[NR_NODES-1];

EXTERN mpa_t mpa, *mpa_ptr;
extern  local_nodeid;
extern char nonprog[];


