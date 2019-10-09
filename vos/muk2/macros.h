
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
		
#define  DVK_sendrec_T(r, d, m, timeout) \
do {\
	long t;\
	t = timeout;\
	do {\
		r = dvk_sendrec_T(d, m, TIMEOUT_NOWAIT); \
		if( r >= 0) break;\
		r = (-errno);\
		if( r == EDVSAGAIN) {\
			if( t > 0) {\
				t -= MUK_DVK_INTERVAL;\
				taskdelay(MUK_DVK_INTERVAL);\
			}\
		}\
	}while( r == EDVSAGAIN);\
}while(0);

#define  DVK_send_T(r, d, m, timeout) \
do {\
	long t;\
	t = timeout;\
	do {\
		r = dvk_send_T(d, m, TIMEOUT_NOWAIT); \
		if( r >= 0) break;\
		r = (-errno);\
		if( r == EDVSAGAIN) {\
			if( t > 0) {\
				t -= MUK_DVK_INTERVAL;\
				taskdelay(MUK_DVK_INTERVAL);\
			}\
		}\
	}while(r == EDVSAGAIN);\
}while(0);
								
#define  DVK_receive_T(r, s, m, timeout) \
do {\
	long t;\
	t = timeout;\
	do {\
		r = dvk_receive_T(s, m, TIMEOUT_NOWAIT); \
		if( r >= 0) break;\
		r = (-errno);\
		if( r == EDVSAGAIN) {\
			if( t > 0) {\
				t -= MUK_DVK_INTERVAL;\
				taskdelay(MUK_DVK_INTERVAL);\
			}else{break;}\
		}\
	}while(r == EDVSAGAIN);\
}while(0);
	
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
			

			

