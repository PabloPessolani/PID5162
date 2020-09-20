#include "dvs_run.h"
   
dc_usr_t  dcu, *dc_ptr;
dvs_usr_t dvs, *dvs_ptr;  
proc_usr_t proc_usr, *proc_ptr;
 
#define	SLEEP_TIME	30
 
void  main ( int argc, char *argv[] )
{
	int dcid, retry, i; 
	int rcode, proc_ep;
	
	USRDEBUG("[%s] argc=%d\n", argv[0], argc);
	for( i = 1; i < argc ; i++) {
		USRDEBUG("[%s] argv[%d]=%s\n", argv[0], i, argv[i]);
	}
	
	rcode = dvk_open();
	if (rcode < 0)  ERROR_PRINT(rcode);

	retry = 0;
	do {
		retry++;
		USRDEBUG("[%s] retry=%d\n", argv[0], retry);
		rcode = dvk_wait4bind_T(1000);
	}while(rcode < EDVSERRCODE);

	USRDEBUG("[%s] bound on endpoint=%d\n", argv[0], rcode);

	sleep(SLEEP_TIME);
}



