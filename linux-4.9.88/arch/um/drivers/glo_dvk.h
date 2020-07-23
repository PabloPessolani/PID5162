
#ifdef CONFIG_UML_DVK

#ifdef DVK_GLOBAL_HERE
#define EXTERN
char *dvk_dev = UML_DVK_DEV;
#else
#define EXTERN extern
extern char *dvk_dev;
#endif

#ifndef CONFIG_DVKIOCTLIPC
EXTERN dvs_usr_t 	dvs;
#else // CONFIG_DVKIOCTLIPC
extern dvs_usr_t 	dvs;
#endif // CONFIG_DVKIOCTLIPC

EXTERN dc_usr_t 	dcu;
EXTERN proc_usr_t 	uml_proc;
EXTERN proc_usr_t 	rdc_proc;
EXTERN int 			dcid; 

EXTERN int 			uml_ep;		// UML KERNEL ENDPOING : from boot parameter
EXTERN int 			rdc_ep;		// RDISK Client ENDPOING :  the lowest USER endpoint 
EXTERN int 			rd_ep;		// RDISK Endpoint (external from UML)  from boot parameter

extern int  dvk_fd; 
extern int local_nodeid;

#endif // CONFIG_UML_DVK 

