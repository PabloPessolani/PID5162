

#define STRINGIFY(s) XSTRINGIFY(s)
#define XSTRINGIFY(s) #s
#pragma message ("CONFIG_UML_DVK=" STRINGIFY(CONFIG_UML_DVK))

#ifdef CONFIG_UML_DVK

#pragma message ("DVK_GLOBAL_HERE=" STRINGIFY(DVK_GLOBAL_HERE))
#ifdef DVK_GLOBAL_HERE
#define EXTERN
char *dvk_dev = UML_DVK_DEV;
#else
#define EXTERN extern
extern char *dvk_dev;
#endif

#ifndef CONFIG_DVKIPC
#pragma message ("EXTERN=" STRINGIFY(EXTERN))
#pragma message ("Defining dvs")
EXTERN dvs_usr_t 	dvs;
#else // CONFIG_DVKIPC
extern dvs_usr_t 	dvs;
#endif // CONFIG_DVKIPC

EXTERN dc_usr_t 	dcu;
EXTERN proc_usr_t 	uml_proc;
EXTERN proc_usr_t 	rdc_proc;
EXTERN int 			dcid; 

EXTERN int 			uml_pid; 	// DVK thread PID 
EXTERN int 			uml_ep;		// UML KERNEL ENDPOINT : from boot parameter
EXTERN int 			rdc_ep;		// RDISK Client ENDPOINT :  the lowest USER endpoint 
EXTERN int 			rd_ep;		// RDISK Endpoint (external from UML)  from boot parameter

EXTERN cmd_t		dvk_cmd;

 
extern int  dvk_fd; 
extern int local_nodeid;

#endif // CONFIG_UML_DVK 

