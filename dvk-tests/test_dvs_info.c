
#include "tests.h"

dvs_usr_t  dvs;
   
void  main ( int argc, char *argv[] )
{
	int c, nodeid, ret;
	dvs_usr_t  *d_ptr; 

	nodeid = (-1);

	d_ptr = &dvs;

	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);
	
    printf("Get DVS info\n");
    nodeid = dvk_getdvsinfo(&dvs);
    printf("local node ID %d... \n", nodeid);
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(d_ptr));
	printf(DVS_MAX_FORMAT, DVS_MAX_FIELDS(d_ptr));
	printf(DVS_VER_FORMAT, DVS_VER_FIELDS(d_ptr));

}



