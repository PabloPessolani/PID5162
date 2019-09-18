#define ERROR_EXIT(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n",__FILE__, __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	taskexit(rcode); \
 }while(0);
 
#define SYSERR(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n",__FILE__, __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
 }while(0)

#define ERROR_PRINT(rcode) SYSERR(rcode)
	 
#define ERROR_RETURN(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n",__FILE__, __FUNCTION__ ,__LINE__,rcode); \
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
		MUX_UNLOCK(y);\
		tasksleep(&x);\
		MUX_LOCK(y);\
		}while(0)	
 
#define COND_SIGNAL(x) do{ \
		taskwakeup(&x);\
		MUKDEBUG("COND_SIGNAL %s\n", #x);\
		}while(0)
			
