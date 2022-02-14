
#include "tests.h"

dvs_usr_t  dvs;

#define YEAR ((((__DATE__ [7] - '0') * 10 + (__DATE__ [8] - '0')) * 10 \
+ (__DATE__ [9] - '0')) * 10 + (__DATE__ [10] - '0'))

#define MONTH (__DATE__ [2] == 'n' ? 0 \
: __DATE__ [2] == 'b' ? 1 \
: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? 2 : 3) \
: __DATE__ [2] == 'y' ? 4 \
: __DATE__ [2] == 'n' ? 5 \
: __DATE__ [2] == 'l' ? 6 \
: __DATE__ [2] == 'g' ? 7 \
: __DATE__ [2] == 'p' ? 8 \
: __DATE__ [2] == 't' ? 9 \
: __DATE__ [2] == 'v' ? 10 : 11)

#define DAY ((__DATE__ [4] == ' ' ? 0 : __DATE__ [4] - '0') * 10 \
+ (__DATE__ [5] - '0'))

#define DATE_AS_INT (((YEAR - 2000) * 12 + MONTH) * 31 + DAY)

   
void  main ( int argc, char *argv[] )
{
	int c, nodeid, ret;
	dvs_usr_t  *d_ptr; 

	nodeid = (-1);

	d_ptr = &dvs;

	ret = dvk_open();
	if (ret < 0)  ERROR_PRINT(ret);
	
	printf("Date %s: date_as_int=%d\n", __DATE__, DATE_AS_INT); 
	
    printf("Get DVS info\n");
    nodeid = dvk_getdvsinfo(&dvs);
    printf("local node ID %d... \n", nodeid);
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(d_ptr));
	printf(DVS_MAX_FORMAT, DVS_MAX_FIELDS(d_ptr));
	printf(DVS_VER_FORMAT, DVS_VER_FIELDS(d_ptr));

}



