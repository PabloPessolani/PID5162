
#include "tests.h"

char *dvk_names[DVK_NR_CALLS] = {
    "void0",
    "dc_init",
    "mini_send",
    "mini_receive",
    "mini_notify",
    "mini_sendrec",
    "mini_rcvrqst",
    "mini_reply",
    "dc_end",
    "bind",
    "unbind",
    "getpriv",
    "setpriv",
    "vcopy",
    "getdcinfo",
    "getprocinfo",
    "mini_relay",
    "proxies_bind",
    "proxies_unbind",
    "getnodeinfo",
    "put2lcl",
    "get2rmt",
    "add_node",
    "del_node",
    "dvs_init",
    "dvs_end",
    "getep",
    "getdvsinfo",
    "proxy_conn",
    "wait4bind",
    "migrate",   
    "node_up",
    "node_down",
    "getproxyinfo",
	"wakeup",
};   /* the ones we are gonna replace */

#define PRINTF_DVK_MAP(map) do {\
	int i, j, index;\
	printf("NR_DVK_CHUNKS=%d\n",NR_DVK_CHUNKS);\
	printf("BITCHUNK_BITS=%d\n",BITCHUNK_BITS);\
	for(i = 0, index=0; i < NR_DVK_CHUNKS; i++){\
		printf("\n%X:",(map.chunk)[i]);\
		for(j = 0; j < BITCHUNK_BITS; j++, index++){ \
			if (get_sys_bit(map, j)) {\
				if(index < DVK_NR_CALLS)\
					printf("[%s(%d)]->", dvk_names[index], index);\
			}\
		}\
	}\
	printf("\n");\
}while(0);

void  main ( int argc, char *argv[] )
{
	proc_usr_t *uproc_ptr;
	priv_usr_t *upriv_ptr;
	int dcid, p_nr, ret;
	
	posix_memalign( (void**) &uproc_ptr, getpagesize(), sizeof(proc_usr_t));
	if (uproc_ptr== NULL) {
		fprintf(stderr, "uproc_ptr posix_memalign\n");
		exit(EXIT_FAILURE);
	}
		
	posix_memalign( (void**) &upriv_ptr, getpagesize(), sizeof(priv_usr_t));
	if (upriv_ptr== NULL) {
		fprintf(stderr, "upriv_ptr posix_memalign\n");
		exit(EXIT_FAILURE);
	}

	if ( argc != 3) {
		fprintf(stderr,  "Usage: %s <dcid> <p_nr> \n", argv[0] );
		exit(EXIT_FAILURE);
	}

	dcid = atoi(argv[1]);
	if ( dcid < 0 || dcid >= NR_DCS) {
		fprintf(stderr,  "Invalid dcid [0-%d]\n", NR_DCS-1 );
		exit(EXIT_FAILURE);
	}

	p_nr = atoi(argv[2]);
	
	ret = dvk_open();
	if(ret < 0) { 
		ERROR_PRINT(ret);
		exit(1);
	}	
	
	// getting process info 
	ret = dvk_getprocinfo(dcid, p_nr, uproc_ptr);
	if(ret < 0) { 
		ERROR_PRINT(ret);
		exit(1);
	}	
	printf(PROC_USR_FORMAT, PROC_USR_FIELDS(uproc_ptr));
	printf(PROC_WAIT_FORMAT, PROC_WAIT_FIELDS(uproc_ptr));
	printf(PROC_COUNT_FORMAT, PROC_COUNT_FIELDS(uproc_ptr));
//	printf(PROC_CPU_FORMAT, PROC_CPU_FIELDS(uproc_ptr));
	
	// getting privileges info 
	ret = dvk_getpriv(dcid , p_nr, upriv_ptr);
	if(ret < 0) { 
		ERROR_PRINT(ret);
		exit(1);
	}		
	printf("sizeof(priv_usr_t)=%d\n" , sizeof(priv_usr_t));
	printf(PRIV_USR_FORMAT, PRIV_USR_FIELDS(upriv_ptr));
	PRINTF_DVK_MAP(upriv_ptr->priv_dvk_allowed);
	
}



