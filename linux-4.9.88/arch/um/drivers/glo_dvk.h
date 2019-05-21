
#ifdef CONFIG_UML_DVK

#ifdef DVK_GLOBAL_HERE
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN dvs_usr_t 	dvs;
EXTERN dc_usr_t 	dcu;
EXTERN proc_usr_t 	uml_proc;
EXTERN int 			dcid; 
EXTERN int 			uml_ep;

extern int  dvk_fd; 
extern int local_nodeid;
#ifdef DVK_GLOBAL_HERE
char *dvk_dev = UML_DVK_DEV;
#else 
extern char *dvk_dev;
#endif 
	

#endif // CONFIG_UML_DVK

