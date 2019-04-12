
#define DVK_VCOPY_FUNCTION

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
	exit(rcode); \
 }while(0);

 #define ERROR_PT_EXIT(rcode) \
 do { \
     	printf("ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
		fflush(stderr);\
		pthread_exit(&rcode);\
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
		MUKDEBUG("MTX_LOCK %s \n", #x);\
		pthread_mutex_lock(&x);\
		}while(0)
			
#define MTX_UNLOCK(x) do{ \
		pthread_mutex_unlock(&x);\
		MUKDEBUG("MTX_UNLOCK %s \n", #x);\
		}while(0)	
			
#define COND_WAIT(x,y) do{ \
		MUKDEBUG("COND_WAIT %s %s\n", #x,#y );\
		pthread_cond_wait(&x, &y);\
		}while(0)	
 
#define COND_SIGNAL(x) do{ \
		pthread_cond_signal(&x);\
		MUKDEBUG("COND_SIGNAL %s\n", #x);\
		}while(0)
			
#ifdef ALLOC_LOCAL_TABLE 			
#define ENDPOINT2PTR(ep) 	&proc_table[_ENDPOINT_P(ep)+dcu.dc_nr_tasks];
#define PROC2PTR(p_nr) 		&proc_table[p_nr+dcu.dc_nr_tasks];
#else /* ALLOC_LOCAL_TABLE */			
#define ENDPOINT2PTR(ep) 	(proc_usr_t *) PROC_MAPPED(_ENDPOINT_P(ep))
#define PROC2PTR(p_nr) 		(proc_usr_t *) PROC_MAPPED(p_nr)
#endif /* ALLOC_LOCAL_TABLE */
#define PROC2PRIV(p_nr) 	&priv_table[p_nr+dcu.dc_nr_tasks];

#define PROC_MAPPED(p_nr) ((( p_nr + dc_ptr->dc_nr_tasks ) * dvs_ptr->d_size_proc) + kproc_map)

