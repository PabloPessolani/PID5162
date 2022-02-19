/**********************************************************************/
/**********************************************************************/

#include "tests.h"

dvs_usr_t dvs, *dvs_ptr;
dc_usr_t dcu,  *dcu_ptr;
extern int local_nodeid;

#define DC_NAMESPACES (CLONE_NEWUTS|CLONE_NEWPID|CLONE_NEWIPC|CLONE_NEWNS|CLONE_NEWNET)
#define DC_CLONE_FLAGS (CLONE_FILES|CLONE_SIGHAND|CLONE_VM)


void  main ( int argc, char *argv[] )
{
	int c, nodeid, dcid, rcode, ret;

	dcu_ptr = &dcu;
	
	if( argc != 2){
		fprintf (stderr,"usage: %s [dcid]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);

	local_nodeid = dvk_getdvsinfo(&dvs);
	if(local_nodeid < 0 )
		ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	printf("local_nodeid=%d\n", local_nodeid);

	dcid = atoi(argv[1]);
	rcode = dvk_getdcinfo(dcid, dcu_ptr);
	if( rcode < 0) ERROR_PRINT(rcode);
	printf(DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));
	printf(DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));
	printf(DC_WARN_FORMAT, DC_WARN_FIELDS(dcu_ptr));
	
	exit(0);
 }



