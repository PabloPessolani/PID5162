
#ifdef RHS_GLOBAL_HERE
#undef  EXTERN
#define EXTERN
#else
#undef  EXTERN
#define EXTERN extern
#endif

EXTERN proc_usr_t  rhs, *rhs_ptr;
EXTERN proc_usr_t  rhc, *rhc_ptr;
EXTERN dc_usr_t  dcu, *dc_ptr;
EXTERN dvs_usr_t dvs, *dvs_ptr;
EXTERN node_usr_t node, *node_ptr;
EXTERN char *rhs_dir;
EXTERN char *rhs_name;
EXTERN unsigned int RHS_mandatory;
EXTERN int dcid;
EXTERN int	dvk_fd;
EXTERN int local_nodeid;
EXTERN message rhs_m;
EXTERN rhc_ep, rhs_ep;
EXTERN int rhs_who_e, rhs_who_p;		/* message source endpoint and proc_table */



