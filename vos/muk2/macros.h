
// #define DVK_VCOPY_FUNCTION

#ifdef DVK_VCOPY_FUNCTION
#define MUK_vcopy(rc, sep, sad, dep, dad, b) \
do{ \
		MUKDEBUG("MUK_vcopy %d->%d bytes=%d \n", sep, dep, b);\
		rc = dvk_vcopy(sep, sad, dep, dad, b) ;\
}while(0);
#else // DVK_VCOPY_FUNCTION
#define MUK_vcopy(rc, sep, sad, dep, dad, b) \
do{ \
		MUKDEBUG("MUK_vcopy %d->%d bytes=%d \n", sep, dep, b);\
		memcpy(dad, sad, b) ;\
		rc = 0;\
}while(0);
#endif // DVK_VCOPY_FUNCTION
		

#define ERROR_EXIT(rcode) \
 do { \
     	printf("ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	taskexitall(rcode);\
 }while(0);

 #define ERROR_TSK_EXIT(rcode) \
 do { \
     	printf("ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
		fflush(stderr);\
		taskexit(rcode);\
 }while(0)
	 
 
 #define ERROR_PRINT(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
 }while(0)
	 
#define SYSERR(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n", __FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
 }while(0)

#define ERROR_RETURN(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n", __FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	return(rcode);\
 }while(0)
   
#define CHECK_P_NR(p_nr)		\
do {\
	if( p_nr < (-dcu.dc_nr_tasks) || p_nr >= dcu.dc_nr_procs) {\
		return(EDVSRANGE);\
	}\
}while(0)
			
#define MTX_LOCK(x) do{ \
		MUKDEBUG("MTX_LOCK %s:%d\n", #x, x);\
		x--;\
		}while(0)
			
#define MTX_UNLOCK(x) do{ \
		x++;\
		MUKDEBUG("MTX_UNLOCK %s:%d\n", #x, x);\
		}while(0)	
			
#define COND_WAIT(x,y) do{ \
		MUKDEBUG("COND_WAIT %s %s\n", #x,#y );\
		MTX_UNLOCK(y);\
		tasksleep(&x);\
		MTX_LOCK(y);\
		}while(0)	
 
#define COND_SIGNAL(x) do{ \
		taskwakeup(&x);\
		MUKDEBUG("COND_SIGNAL %s\n", #x);\
		}while(0)
			

