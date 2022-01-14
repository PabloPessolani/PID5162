
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

EXTERN	char Spread_name[80];
EXTERN  sp_time ddp_timeout;

EXTERN clockid_t clk_id;
EXTERN local_nodeid;
EXTERN ddp_t ddp;

EXTERN cmd_input[MAX_CMDIN_LEN];
EXTERN cmd_output[MAX_CMDOUT_LEN];


