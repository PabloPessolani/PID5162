
#include "dvs_run.h"

#ifdef USRDBG
#pragma message ("USRDBG=YES")
#else //  USRDBG
#pragma message ("USRDBG=NO")
#endif  //  USRDBG

extern char *optarg;
extern int optind, opterr, optopt;

dc_usr_t  dcu, *dc_ptr;
dvs_usr_t dvs, *dvs_ptr;
int local_nodeid;
proc_usr_t proc_usr, *proc_ptr;
int dcid, proc_ep, proc_nr, migr_type;
int proc_pid, new_nodeid;

#define NOT_MIGRATE (-1) 

/*===========================================================================*
 *				get_dvs_params				     *
 *===========================================================================*/
void get_dvs_params(void)
{
	USRDEBUG("\n");
	local_nodeid = dvk_getdvsinfo(&dvs);
	USRDEBUG("local_nodeid=%d\n",local_nodeid);
	if( local_nodeid < DVS_NO_INIT) ERROR_EXIT(local_nodeid);
	dvs_ptr = &dvs;
	USRDEBUG(DVS_USR_FORMAT, DVS_USR_FIELDS(dvs_ptr));
}

/*===========================================================================*
 *				get_dc_params				     *
 *===========================================================================*/
void get_dc_params(int dcid)
{
	int rcode;

	USRDEBUG("dcid=%d\n", dcid);
	if ( dcid < 0 || dcid >= dvs.d_nr_dcs) {
 	        fprintf(stderr,"Invalid dcid [0-%d]\n", dvs.d_nr_dcs );
 	        ERROR_EXIT(EDVSBADDCID);
	}
	rcode = dvk_getdcinfo(dcid, &dcu);
	if( rcode ) ERROR_EXIT(rcode);
	dc_ptr = &dcu;
	USRDEBUG(DC_USR1_FORMAT, DC_USR1_FIELDS(dc_ptr));
	USRDEBUG(DC_USR2_FORMAT, DC_USR2_FIELDS(dc_ptr));
}

void print_usage(char *argv0){
	fprintf(stderr,"Usage: %s -<s|r|c> <dcid> <endpoint> [<new_nodeid>] [<pid>]\n");
	fprintf(stderr,"\t\t s: Start \n");
	fprintf(stderr,"\t\t r: Rollback \n");
	fprintf(stderr,"\t\t c: Commit \n");
	fprintf(stderr,"<dcid>: DC ID for the process\n");
	fprintf(stderr,"<endpoint>: Endpoint of the migrating process\n");
	fprintf(stderr,"<new_nodeid>: on Commit, the new node ID of the migrated process\n");
	fprintf(stderr,"<pid>: on Commit, the PID of the migrated process if the new_nodeid is local_nodeid\n");
	fprintf(stderr,"      It can be (0) if the local process is bound as a REMOTE BACKUP\n");	
	ERROR_EXIT(EDVSINVAL);	
}

/*===========================================================================*
 *				main					     					*
 *===========================================================================*/
int main ( int argc, char *argv[] )
{
	int rcode, opt, i, first_arg, arg_c;
	char **arg_v;
	pid_t child_pid;
	
	if ( argc != 4 &&  argc != 5  &&  argc != 6 ) {
		USRDEBUG("argc=%d\n", argc);
		for( i = 0; i < argc ; i++) {
			USRDEBUG("argv[%d]=%s\n", i, argv[i]);
		}
		print_usage(argv[0]);
 	    exit(1);
    }

	rcode = dvk_open();
	if (rcode < 0)  ERROR_EXIT(rcode);
	
	get_dvs_params();
	
	migr_type = NOT_MIGRATE;
    while ((opt = getopt(argc, argv, "src")) != -1) {
		if( migr_type != NOT_MIGRATE)
			print_usage(argv[0]);
        switch (opt) {
			case 's':
				migr_type = MIGR_START;
				break;
			case 'r':
				migr_type = MIGR_ROLLBACK;
				break;
			case 'c':
				if( argc < 5)
					print_usage(argv[0]);
				new_nodeid = atoi(argv[4]);
				USRDEBUG("new_nodeid=%d\n", new_nodeid)
				if ( new_nodeid == local_nodeid){
					if ( argc < 6)
						print_usage(argv[0]);
					proc_pid = atoi(argv[5]); 
				}
				USRDEBUG("proc_pid=%d\n", proc_pid)
				migr_type = MIGR_COMMIT;
				break;
			default: /* '?' */
				print_usage(argv[0]);
				break;
        }
    }
	
	USRDEBUG("getopt migr_type=%d\n", migr_type);
	if( migr_type == NOT_MIGRATE ) {
		print_usage(argv[0]);
	}
		
	dcid = atoi(argv[2]);
	USRDEBUG("dcid=%d\n", dcid);
	get_dc_params(dcid);

if(	migr_type == MIGR_COMMIT) {
	printf("please wait1...\n");
	sleep(2); /// NO SACAR, vaya a saber porque no funciona si se lo saca.
}			
	proc_ep = atoi(argv[3]);
	USRDEBUG("proc_ep=%d\n", proc_ep);

	proc_nr = _ENDPOINT_P(proc_ep);
	if ( (proc_nr < (-dc_ptr->dc_nr_tasks)) 
	|| (proc_nr >= (dc_ptr->dc_nr_procs-dc_ptr->dc_nr_tasks))) {
 	    fprintf(stderr, "Invalid <endpoint> [%d-%d]\n", 
			(-dc_ptr->dc_nr_tasks), 
			(dc_ptr->dc_nr_procs-dc_ptr->dc_nr_tasks));
		ERROR_EXIT(EDVSBADPROC);
	}

	if( !TEST_BIT(dc_ptr->dc_nodes, new_nodeid)) {
 	    fprintf(stderr, "DC %d cannot run on node %d\n", dcid, new_nodeid);
		ERROR_EXIT(EDVSDCNODE);
	}
	
	USRDEBUG("migr_type=%d argc=%d \n", migr_type, argc);
	
if(	migr_type == MIGR_COMMIT) {
	printf("please wait2...\n");
	sleep(2); /// NO SACAR, vaya a saber porque no funciona si se lo saca.
}
	switch(migr_type){
		case MIGR_START:
			rcode = dvk_migr_start(dcid, proc_ep);
			break;
		case MIGR_ROLLBACK:
			rcode = dvk_migr_rollback(dcid, proc_ep);
			break;
		case MIGR_COMMIT:
			if(proc_pid == 0) proc_pid = PROC_NO_PID;
			rcode = dvk_migr_commit(proc_pid, dcid, proc_ep, new_nodeid);
			break;
		default:
			USRDEBUG("NEVER HERE!!\n");
			rcode = EDVSINVAL;
			break;
	}
	if( rcode < 0) ERROR_EXIT(rcode);
	exit(0);
}

	
