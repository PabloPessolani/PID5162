/* EXTERN should be extern except for the table file */
#ifdef _TABLE
	#ifdef EXTERN
		#undef EXTERN 
		#define EXTERN
	#endif
#else
	#ifdef EXTERN
		#undef EXTERN 
		#define EXTERN extern
	#endif		
#endif

//EXTERN int local_nodeid;
EXTERN int dcid;
EXTERN unsigned int bm_irq;			// virtual IRQ bitmap.
EXTERN fd_set fdset;
	
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN pid_t eth_lpid;
EXTERN pid_t main_lpid;
EXTERN dc_usr_t  dcu, *dc_ptr;

EXTERN proc_usr_t eth, *eth_ptr;	
EXTERN message *mi_ptr, *mo_ptr;

EXTERN eth_card_t ec_table[EC_PORT_NR_MAX];

/* =============== global variables =============== */
EXTERN int rx_slot_nr = 0;          /* Rx-slot number */
EXTERN int tx_slot_nr = 0;          /* Tx-slot number */
EXTERN int cur_tx_slot_nr = 0;      /* Tx-slot number */
EXTERN char isstored[TX_RING_SIZE]; /* Tx-slot in-use */
EXTERN char *progname;
EXTERN long clockTicks;
EXTERN unsigned int iface_rqst;
EXTERN unsigned int rqst_source;

EXTERN pthread_t eth_tid;



	