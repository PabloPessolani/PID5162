
#define ERROR_EXIT(rcode) \
 do { \
     	printf("ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	exit(rcode); \
 }while(0);


 #define ERROR_PRINT(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
 }while(0)
	 

#define ERROR_RETURN(rcode) \
 do { \
     	fprintf(stderr,"ERROR: %s:%s:%u: rcode=%d\n", __FILE__ , __FUNCTION__ ,__LINE__,rcode); \
	fflush(stderr);\
	return(rcode);\
 }while(0)
   
#define MUK2_RETURN(rcode) \
 do { \
     	printf("MUK2_RETURN: %s:%s:%u: rcode=%d\n",__FILE__ , __FUNCTION__ ,__LINE__,rcode); \
		fflush(stderr);\
		longjmp(sched_env, rcode);\
}while(0);
	 
#define BUILD_NOTIFY_MSG(d_ptr, s_nr) \
 do { \
    d_ptr->p_msg->m_source = HARDWARE;\
	d_ptr->p_msg->m_type = NOTIFY_FROM(s_nr);\
	clock_gettime(CLOCK_REALTIME, &d_ptr->p_msg->NOTIFY_TIMESTAMP);\
	d_ptr->p_msg->NOTIFY_ARG = (long) d_ptr->p_pending;\
}while(0); 
 