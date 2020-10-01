
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
EXTERN message m_in;		/* the input message itself */
EXTERN message m_out;		/* the output message used for reply */
EXTERN message *m_ptr;		/* pointer to message */

EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN dc_usr_t  dcu[NR_DCS], *dc_ptr[NR_DCS];

EXTERN int local_nodeid;
EXTERN int nr_control;		// total number of endpoints to control
EXTERN radar_t	*rad_ptr[NR_MAX_CONTROL];

EXTERN	char Spread_name[80];
EXTERN  sp_time test_timeout;
EXTERN  int systask_flag;			// is SYSTASK running?
EXTERN  int svc_nr;



