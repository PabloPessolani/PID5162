
#define ERROR_EXIT(rcode) \
 do { \
     	printf("ERROR: %s:%s:%u: rcode=%d\n",__FILE__ ,__FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	exit(rcode); \
 }while(0);

 #define ERROR_PRINT(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n", __FILE__ ,__FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
 }while(0)

#define ERROR_RETURN(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n", __FILE__ ,__FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	return(rcode);\
 }while(0)
   
 #define ERROR_PT_EXIT(rcode) \
 do { \
     	printf("ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
		fflush(stderr);\
		pthread_exit(&rcode);\
 }while(0)
	 
 #define MTX_LOCK(x) do{ \
		PXYDEBUG("MTX_LOCK %s \n", #x);\
		pthread_mutex_lock(&x);\
		}while(0)
			
#define MTX_UNLOCK(x) do{ \
		pthread_mutex_unlock(&x);\
		PXYDEBUG("MTX_UNLOCK %s \n", #x);\
		}while(0)	
			
#define COND_WAIT(x,y) do{ \
		PXYDEBUG("COND_WAIT %s %s\n", #x,#y );\
		pthread_cond_wait(&x, &y);\
		}while(0)	

#define COND_WAIT_T(r,x,y,t) do{ \
		PXYDEBUG("COND_WAIT_T %s %s\n", #x,#y);\
		r = pthread_cond_timedwait(&x, &y, t);\
		}while(0)
 
#define COND_SIGNAL(x) do{ \
		pthread_cond_signal(&x);\
		PXYDEBUG("COND_SIGNAL %s\n", #x);\
		}while(0)
			
