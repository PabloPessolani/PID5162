
#ifdef _TABLE
	#ifdef EXTERN
	#undef EXTERN
	#endif 
#define EXTERN
	char rname_any[]="ANY";
	clockid_t clk_id = CLOCK_REALTIME;

#else
	#ifdef EXTERN
	#undef EXTERN
	#endif 
#define EXTERN extern
	extern char rname_any[];
	extern clockid_t clk_id;

#endif

/* The parameters of the call are kept here. */
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN proxy_t 	proxy_tab[NR_NODES];

EXTERN sess_tab_t sess_table[NR_DCS]; // Session Table  
EXTERN int nr_sess_entries;				// Constant !! Number of session entries per table .

EXTERN int local_nodeid;
extern char nonprog[];
EXTERN  service_t 	service_tab[MAX_SVC_NR];

EXTERN lb_t  lb;
EXTERN lb_t *lb_ptr;

EXTERN dvs_usr_t dvs;   
EXTERN proxies_usr_t px;
EXTERN jiff  cuse[2],  cice[2], csys[2],  cide[2], ciow[2],  cxxx[2],   cyyy[2],   czzz[2];
EXTERN int	idx;
EXTERN struct timespec ts;

extern int errno;

static const LZ4F_preferences_t lz4_preferences = {
	{ LZ4F_max1MB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame, 0, { 0, 0 } },
	0,   /* compression level */
	0,   /* autoflush */
	{ 0, 0, 0, 0 },  /* reserved, must be set to 0 */
};



