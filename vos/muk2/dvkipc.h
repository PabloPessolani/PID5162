
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
	
#ifdef DVK_IPC_ANULADO	
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
	
#endif // DVK_IPC_ENABLE
	
			

			

