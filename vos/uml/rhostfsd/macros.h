
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
   



