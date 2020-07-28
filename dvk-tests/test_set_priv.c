
#include "tests.h"

char *dvk_names[DVK_NR_CALLS] = {
    "void0",
    "dc_init",
    "send",
    "receive",
    "notify",
    "sendrec",
    "rcvrqst",
    "reply",
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
			if (get_sys_bit(map, index)) {\
				if(index < DVK_NR_CALLS)\
					printf("[%s(%d)]->", dvk_names[index], index);\
			}\
		}\
	}\
	printf("\n");\
}while(0);

#define PRINTF_SYS_MAP(map) do {\
	int i;\
	printf("NR_SYS_CHUNKS=%d\n",NR_SYS_CHUNKS);\
	printf("BITCHUNK_BITS=%d\n",BITCHUNK_BITS);\
	for(i = 0; i < NR_SYS_CHUNKS; i++){\
		printf("\n%X:",(map.chunk)[i]);\
	}\
	printf("\n");\
}while(0);

dvs_usr_t dvs, *dvs_ptr;
extern int local_nodeid;
dc_usr_t dcu, *dcu_ptr; 

void  main ( int argc, char *argv[] )
{
	proc_usr_t *uproc_ptr;
	priv_usr_t *upriv_ptr;
	int i, c, ret;

	extern char *optarg;
	extern int optind, optopt, opterr;
	
	int warn_ep = -1;
	int level   = -1;
	int ipc_to  = -1;
	int ipc_not = -1;
	int allow_dvk = -1;
	int deny_dvk = -1;
	int dcid = -1;
	int p_nr = -1;
	char *allow_text = NULL;
	char *deny_text = NULL;
	int changed = 0;

	posix_memalign( (void**) &dvs_ptr, getpagesize(), sizeof(dvs_usr_t));
	if (dvs_ptr== NULL) {
		fprintf(stderr, "dvs_ptr posix_memalign\n");
		exit(EXIT_FAILURE);
	}
	
	posix_memalign( (void**) &dcu_ptr, getpagesize(), sizeof(dc_usr_t));
	if (dcu_ptr== NULL) {
		fprintf(stderr, "dcu_ptr posix_memalign\n");
		exit(EXIT_FAILURE);
	}

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
	
	ret = dvk_open();
	if(ret < 0) { 
		ERROR_PRINT(ret);
		exit(1);
	}	
	
	local_nodeid = dvk_getdvsinfo(dvs_ptr);
	if(local_nodeid < 0 )
		ERROR_EXIT(local_nodeid);
	printf(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
	printf("local_nodeid=%d\n", local_nodeid);
	
	while ((c = getopt(argc, argv, "d:p:w:l:I:A:D:N:")) != -1) {
		switch(c) {
			case 'd':
				dcid = atoi(optarg);
				if( dcid < 0 || dcid >= dvs_ptr->d_nr_dcs) {
					fprintf (stderr, "Invalid dcid [0-%d]\n", dvs_ptr->d_nr_dcs-1 );
					exit(EXIT_FAILURE);
				}
				break;
			case 'p':
				p_nr = atoi(optarg);
				break;
			case 'w':
				warn_ep = atoi(optarg);
				if( warn_ep <= -NR_TASKS || warn_ep  >= (NR_SYS_PROCS-NR_TASKS)) {
					fprintf (stderr, "Invalid warn_ep [%d,%d]\n", -NR_TASKS+1, (NR_SYS_PROCS-NR_TASKS-1) );
					exit(EXIT_FAILURE);
				}
				break;
			case 'l':
				level = atoi(optarg);
				if(level < 0 || level  >= PROXY_PRIV ) {
					fprintf (stderr, "Invalid level [0-%d]\n", PROXY_PRIV-1);
					exit(EXIT_FAILURE);
				}
				break;
			case 'N':
				ipc_not = atoi(optarg);
				if( ipc_not < -NR_TASKS || ipc_not  >= (NR_SYS_PROCS-NR_TASKS) ) {
					fprintf (stderr, "Invalid ipc_not [%d,%d]\n", -NR_TASKS+1, (NR_SYS_PROCS-NR_TASKS-1) );
					exit(EXIT_FAILURE);
				}
				break;					
			case 'I':
				ipc_to = atoi(optarg);
				if( ipc_to < -NR_TASKS || ipc_to  >= (NR_SYS_PROCS-NR_TASKS) ) {
					fprintf (stderr, "Invalid ipc_to [%d,%d]\n", -NR_TASKS+1, (NR_SYS_PROCS-NR_TASKS-1) );
					exit(EXIT_FAILURE);
				}
				break;
			case 'A':
				allow_text = optarg;
				for( i = 0; i < DVK_NR_CALLS; i++){
					if(strcasecmp(dvk_names[i], allow_text) == 0)
						break;
				}
				if( i == DVK_NR_CALLS){
					fprintf (stderr, "Valid Allowed DVK Calls are:\n");
					for( i = 0; i < DVK_NR_CALLS; i++){
						fprintf (stderr, "%s,", dvk_names[i]);
					}					
					exit(EXIT_FAILURE);
				}
				allow_dvk=i;
				printf("deny_dvk=%d\n", allow_dvk);
				break;	
			case 'D':
				deny_text = optarg;
				for( i = 0; i < DVK_NR_CALLS; i++){
					if( strcasecmp(dvk_names[i], deny_text) == 0)
						break;
				}
				if( i == DVK_NR_CALLS){
					fprintf (stderr, "Valid Denied DVK Calls are:\n");
					for( i = 0; i < DVK_NR_CALLS; i++){
						fprintf (stderr, "%s,", dvk_names[i]);
					}					
					exit(EXIT_FAILURE);
				}
				deny_dvk=i;
				printf("deny_dvk=%d\n", deny_dvk);
				break;
			default:
				fprintf (stderr,"usage: %s \n\t [-i id] [-w warn_ep] [-l level] [-I ipc_to] [-N ipc_not] [-A Allowed] [-D Deny] -d dcid -p p_nr"
						, argv[0]);
				exit(EXIT_FAILURE);
		}
		changed = 1;
	}
	
	if( argv[optind] != NULL || dcid == (-1) || p_nr == (-1)){
		fprintf (stderr,"usage: %s \n\t [-i id] [-w warn_ep] [-l level] [-I ipc_to] [-N ipc_not] [-A Allowed] [-D Deny] -d dcid -p p_nr"
			, argv[0]);
		exit(EXIT_FAILURE);
	}	
	
	printf("dcid=%d\n", dcid);
	ret = dvk_getdcinfo(dcid, dcu_ptr);
	if( ret < 0) ERROR_PRINT(ret);
	printf(DC_USR1_FORMAT, DC_USR1_FIELDS(dcu_ptr));
	printf(DC_USR2_FORMAT, DC_USR2_FIELDS(dcu_ptr));
	printf(DC_WARN_FORMAT, DC_WARN_FIELDS(dcu_ptr));

	if( (p_nr < -dcu_ptr->dc_nr_tasks) || p_nr  >= (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks)) {
		fprintf (stderr, "Invalid p_nr [%d,%d]\n", 
			(-dcu_ptr->dc_nr_tasks+1), (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks-1) );
		exit(EXIT_FAILURE);
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
	printf("BEFORE " PRIV_USR_FORMAT, PRIV_USR_FIELDS(upriv_ptr));
	printf("BEFORE \n");
	PRINTF_DVK_MAP(upriv_ptr->priv_dvk_allowed);
	PRINTF_SYS_MAP(upriv_ptr->priv_ipc_to);
	if( changed == 0) exit(0);
	
	if ( warn_ep != -1) {
		if( (warn_ep < -dcu_ptr->dc_nr_tasks) || warn_ep  >= (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks)) {
			fprintf (stderr, "Invalid warn_ep [%d,%d]\n", 
			(-dcu_ptr->dc_nr_tasks+1), (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks-1) );
			exit(EXIT_FAILURE);
		}
		upriv_ptr->priv_warn = warn_ep;
	}
	if ( level != -1) {
		upriv_ptr->priv_level = level;
	}
	if ( ipc_to != -1) {
		if( (ipc_to < -dcu_ptr->dc_nr_tasks) || ipc_to  >= (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks)) {
			fprintf (stderr, "Invalid ipc_to [%d,%d]\n", 
				(-dcu_ptr->dc_nr_tasks+1), (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks-1) );
			exit(EXIT_FAILURE);
		}
		set_sys_bit(upriv_ptr->priv_ipc_to,ipc_to+dcu_ptr->dc_nr_tasks);
	}
	
	if ( ipc_not != -1) {
		if( (ipc_not < -dcu_ptr->dc_nr_tasks) || ipc_not  >= (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks)) {
			fprintf (stderr, "Invalid ipc_not [%d,%d]\n", 
				(-dcu_ptr->dc_nr_tasks+1), (dcu_ptr->dc_nr_sysprocs-dcu_ptr->dc_nr_tasks-1) );
			exit(EXIT_FAILURE);
		}
		unset_sys_bit(upriv_ptr->priv_ipc_to,ipc_not+dcu_ptr->dc_nr_tasks);
	}
	
	if ( allow_dvk != -1) {
		set_sys_bit(upriv_ptr->priv_dvk_allowed, allow_dvk);
	}	
	if ( deny_dvk != -1) {
		unset_sys_bit(upriv_ptr->priv_dvk_allowed, deny_dvk);
	}	

	ret = dvk_setpriv(dcid , p_nr, upriv_ptr);
	if(ret < 0) { 
		ERROR_PRINT(ret);
		exit(1);
	}	
	
	// getting privileges info 
	ret = dvk_getpriv(dcid , p_nr, upriv_ptr);
	if(ret < 0) { 
		ERROR_PRINT(ret);
		exit(1);
	}		
	printf("sizeof(priv_usr_t)=%d\n" , sizeof(priv_usr_t));
	printf("AFTER " PRIV_USR_FORMAT, PRIV_USR_FIELDS(upriv_ptr));
	printf("AFTER \n");
	PRINTF_DVK_MAP(upriv_ptr->priv_dvk_allowed);
	PRINTF_SYS_MAP(upriv_ptr->priv_ipc_to);
	
}



