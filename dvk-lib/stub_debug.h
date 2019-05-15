#define LIBDBG		1

#define NODEBUG  	0x00000000
#define DBGLVL0  	0x00000001
#define GENERIC  	0x00000002
#define INTERNAL  	0x00000004
#define DBGLVL3  	0x00000008
#define DBGLVL4  	0x00000010
#define DBGLVL5  	0x00000020
#define DBGLVL6  	0x00000040
#define DBGLVL7  	0x00000080

#define DBGPROCLOCK  0x00000100
#define DBGDCLOCK  	 0x00000200
#define DBGNODELOCK  0x00000400
#define DBGTASKLOCK  0x00000800

#define DBGMESSAGE	0x00001000
#define DBGCMD		0x00002000
#define DBGVCOPY	0x00004000
#define DBGPARAMS	0x00008000

#define DBGPROC		0x00010000
#define DBGPRIV		0x00020000
#define DBGPROCSEM	0x00040000
#define DBGNODE		0x00080000
#define DBGPROXYLOCK	0x00100000
#define DBGREFCOUNT	0x00200000

#ifdef CONFIG_UML_DVK
#define LIBDEBUG	DVKDEBUG
#define getpid		os_getpid
#define printf		printk

#define ERROR_RETURN(rcode) \
 do { \
     	printf("ERROR: %d:%s:%u: rcode=%d\n",getpid(), __FUNCTION__ ,__LINE__,rcode); \
	return(rcode); \
 }while(0);

#else // CONFIG_UML_DVK
 
 #define ERROR_RETURN(rcode) \
 do { \
     	fprintf(stderr, "ERROR: %d:%s:%u: rcode=%d\n",getpid(), __FUNCTION__ ,__LINE__,rcode); \
	return(rcode); \
 }while(0);
 
#endif // CONFIG_UML_DVK

#ifdef LIBDBG
#define LIBDEBUG(dbglvl, text, args ...) \
   printf("DEBUG %d:%s:%u: " \
             text, getpid() , __FUNCTION__ ,__LINE__, ## args); 
#else 
#define LIBDEBUG(x, args ...)
#endif 

#define PRINT_RETURN(rcode) \
 do { \
     	printf("DEBUG: %d:%s:%u: rcode=%d\n",getpid(), __FUNCTION__ ,__LINE__,rcode); \
	return(rcode); \
 }while(0); 
